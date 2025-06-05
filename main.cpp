#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

constexpr size_t ALIGN = 4096;
constexpr size_t FRAME = ALIGN * 2;
constexpr size_t BLOCKSIZE = FRAME * FRAME;

void readChunk(int fd, off_t offset, size_t threadId,
                std::unordered_map<std::string, int>& localMap)
{
    off_t startOffset = std::max<off_t>(0, offset - FRAME);
    size_t readSize = ((offset == 0) ? 0 : FRAME) + BLOCKSIZE;
    void* alignedBuf;

    if (posix_memalign(&alignedBuf, ALIGN, readSize) != 0) {
        std::cerr << "Thread " << threadId << ": posix_memalign failed\n";
        return;
    }

    ssize_t bytesRead = pread(fd, alignedBuf, readSize, startOffset);
    if (bytesRead < 0) {
        std::cerr << "Thread " << threadId << ": Error reading - " << strerror(errno) << "\n";
        free(alignedBuf);
        return;
    }

    char* data = static_cast<char*>(alignedBuf);
    size_t startPos = (offset == 0) ? 0 : FRAME-1;

    // Look back at incomplete word at start
    if (offset != 0) {
        while (startPos > 0 && !std::isspace(static_cast<unsigned char>(data[startPos]))) {
            --startPos;
        }
    }
    size_t endPos = bytesRead-1;

    // Ignore incomplete word at end
    while (endPos > startPos && !std::isspace(static_cast<unsigned char>(data[endPos]))) {
        --endPos;
    }

    std::string word;
    word.reserve(64);
    for (size_t i = startPos; i <= endPos; ++i) {
        char c = data[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!word.empty()) {
                ++localMap[word];
                word.clear();
            }
        } else {
            word += c;
        }
    }
    free(alignedBuf);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    const char* filename = argv[1];
    int numThreads = sysconf(_SC_NPROCESSORS_ONLN);
    if (numThreads <= 0) {
        numThreads = 4;
    }
    int fd = open(filename, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        std::cerr << "Failed to open file: " << strerror(errno) << "\n";
        return 1;
    }
    auto begin = std::chrono::steady_clock::now();

    off_t fileSize = lseek(fd, 0, SEEK_END);

    std::vector<off_t> chunks;
    for (off_t offset = 0; offset < fileSize; offset += BLOCKSIZE) {
        chunks.push_back(offset);
    }

    std::unordered_map<std::string, int> threadMaps[numThreads];
    std::vector<std::thread> threads;

    std::atomic<size_t> chunkIndex = 0;
    auto getNextChunkIdx = [&]()->size_t {
        return chunkIndex.fetch_add(1);
    };

    for (int threadId = 0; threadId < numThreads; ++threadId) {
        threads.emplace_back([&, threadId]() {
            for (;;)
            {
                if (size_t idx = getNextChunkIdx(); idx < chunks.size()) {
                    readChunk(fd, chunks[idx], threadId, threadSets[threadId]);
                }
                else {
                    break;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    close(fd);

    std::unordered_map<std::string, int> globalMap;
    for (const auto& localMap : threadMaps) {
        for (const auto& [word, count] : localMap) {
            globalMap[word] += count;
        }
    }

    size_t totalWords = 0;
    for (const auto& [word, count] : globalMap) {
        totalWords += count;
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    std::cout << "The time: " << elapsed_ms.count() << " ms\n";
    std::cout << "Total words counted: " << totalWords << "\n";
    std::cout << "\n=== Final unique word count: " << globalMap.size() << " ===\n";

    return 0;
}

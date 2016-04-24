#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <direct.h>
#include <sys/stat.h>
#include "zlib.h"
#include "zpipe.h"

using namespace std;

int isExist(const std::string& filePath);

int mkdirRecursive(const string& path);

bool is230later(const unsigned char* buffer);

void aread(unsigned char* dst, const unsigned char* src, unsigned long long& idx, const unsigned long long& size);

void readUnsignedLongLong(const unsigned char* src, unsigned long long& idx, unsigned long long& dst);

void zlibUncompress(unsigned char* dst, unsigned long long dstLen, unsigned char* src, unsigned long long srcLen);

void replacePathDelimiter(std::string& src);

std::string getFilepath(const std::string& filename);

int main(int args, const char** argv) {
    setlocale(LC_ALL, "ja_JP.UTF-8");
    if (args == 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"))) {
        fprintf(stderr, "usage: %s <xp3 input file name> <output directory name>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    if (args != 3) {
        fprintf(stderr, "usage: %s <xp3 input file name> <output directory name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    std::string myArgv[3];
    for (int i = 0; i < 3; ++i) {
        myArgv[i] = argv[i];
    }
    replacePathDelimiter(myArgv[1]);
    replacePathDelimiter(myArgv[2]);
    // check input file
    if (isExist(myArgv[1]) != 1) {
        fprintf(stderr, "%s: not a regular file\n", myArgv[1].c_str());
        exit(EXIT_FAILURE);
    }
    // check directory
    if (isExist(myArgv[2]) != 0 && isExist(myArgv[2]) != 2) {
        fprintf(stderr, "%s: directory or file may already exist, or has no permission\n", myArgv[2].c_str());
        exit(EXIT_FAILURE);
    }
    int mkdirRet = _mkdir(myArgv[2].c_str());
    if (mkdirRet < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "%s: %s\n", myArgv[2].c_str(), strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    std::vector<std::string> errorFileList;
    FILE* fp = fopen(myArgv[1].c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "%s: %s\n", myArgv[1].c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }
    unsigned char buffer[2048];
    // header
    fread(buffer, sizeof(unsigned char), 8, fp); // xp3 header 1
    fread(buffer, sizeof(unsigned char), 3, fp); // xp3 header 2
    fread(buffer, sizeof(unsigned char), 8, fp); // cushion index
    if (!is230later(buffer)) {
        fprintf(stderr, "%s: not 2.30 later format\n", myArgv[1].c_str());
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fread(buffer, sizeof(unsigned char), 4, fp); // header minor version
    fread(buffer, sizeof(unsigned char), 1, fp); // cushion header
    fread(buffer, sizeof(unsigned char), 8, fp); // index size
    // file manager address
    unsigned long long fileManagerAddress = 0u;
    fread(&fileManagerAddress, sizeof(unsigned long long), 1, fp);
    fseek(fp, fileManagerAddress, SEEK_SET);
    // file manager compress flag
    fread(buffer, sizeof(unsigned char), 1, fp);
    // file manager size
    unsigned long long fileManagerSizeCompressed, fileManagerSizeOriginal;
    fread(&fileManagerSizeCompressed, sizeof(unsigned long long), 1, fp);
    fread(&fileManagerSizeOriginal, sizeof(unsigned long long), 1, fp);
    unsigned char* fileInfo = new unsigned char[fileManagerSizeOriginal];
    // if buffer[0] == 1, content was compressed with zlib, decompress it
    // else, save directly
    if (buffer[0] == 1) {
        unsigned char* fileInfoCompressed = new unsigned char[fileManagerSizeCompressed];
        fread(fileInfoCompressed, sizeof(unsigned char), fileManagerSizeCompressed, fp);
        zlibUncompress(fileInfo, fileManagerSizeOriginal, fileInfoCompressed, fileManagerSizeCompressed);
        delete[] fileInfoCompressed;
    }
    else {
        fread(fileInfo, sizeof(unsigned char), fileManagerSizeOriginal, fp);
    }
    // for each file in file manager
    unsigned long long fileInfoIdx = 0;
    while (fileInfoIdx < fileManagerSizeOriginal) {
        fileInfoIdx += 4; // "File"
        unsigned long long fileManagerSizeLocal; // this file manager size, 8 bytes
        readUnsignedLongLong(fileInfo, fileInfoIdx, fileManagerSizeLocal);
        fileInfoIdx += 4; // "info"
        unsigned long long infoSize;
        readUnsignedLongLong(fileInfo, fileInfoIdx, infoSize);
        fileInfoIdx += 4; // protect flag
        unsigned long long fileSizeOriginal; // file size decompressed
        readUnsignedLongLong(fileInfo, fileInfoIdx, fileSizeOriginal);
        unsigned long long fileSizeCompressed; // file size compressed
        readUnsignedLongLong(fileInfo, fileInfoIdx, fileSizeCompressed);
        unsigned long long fileNameLength = 0u; // file name size in wchar
        for (int i = 0; i < 2; ++i) {
            fileNameLength |= (fileInfo[fileInfoIdx++] & 0xFF) << (i * 8);
        }
        // deal with filename
        wchar_t *fileNameStr = new wchar_t[fileNameLength + 1];
        for (unsigned long long offset = 0; offset < fileNameLength * 2; offset += 2) {
            char fileNameBuffer[2];
            fileNameBuffer[0] = fileInfo[fileInfoIdx + offset + 1];
            fileNameBuffer[1] = fileInfo[fileInfoIdx + offset];
            if (fileNameBuffer[0]) {
                mbtowc(fileNameStr + offset / 2, fileNameBuffer, 2);
            }
            else{
                mbtowc(fileNameStr + offset / 2, fileNameBuffer + 1, 1);
            }
        }
        fileNameStr[fileNameLength] = '\0';
        wprintf(L"%ls\n", fileNameStr);
        fileInfoIdx += fileNameLength * 2;
        fileInfoIdx += 4; // "segm"
        unsigned long long segmentSize; // segment size(#segment * 28)
        readUnsignedLongLong(fileInfo, fileInfoIdx, segmentSize);
        printf("segment size = %llu, #segment = %llu\n", segmentSize, segmentSize / 28);
        unsigned char* segment = new unsigned char[segmentSize];
        for (int i = 0; i < static_cast<int>(segmentSize / 28); ++i) {
            printf("segment %d/%d:\n", i + 1, static_cast<int>(segmentSize / 28));
            aread(segment, fileInfo, fileInfoIdx, 4); // compress flag
            unsigned long long offset;
            unsigned long long fileOriginalSize;
            unsigned long long fileCompressedSize;
            readUnsignedLongLong(fileInfo, fileInfoIdx, offset);
            readUnsignedLongLong(fileInfo, fileInfoIdx, fileOriginalSize);
            readUnsignedLongLong(fileInfo, fileInfoIdx, fileCompressedSize);
            printf("size: original: %llu, compressed: %llu\n", fileOriginalSize, fileCompressedSize);
            char* msFilenameStr = new char[fileNameLength * 2 + 10];
            wcstombs(msFilenameStr, fileNameStr, fileNameLength + 1);
            std::string filenamePrefix = myArgv[2];
            if (filenamePrefix.back() != '/') {
                filenamePrefix += "/";
            }
            std::string filenamePostfix = "_temp";
            std::string filenameTemp = msFilenameStr;
            filenameTemp = filenamePrefix + filenameTemp + filenamePostfix;
            std::string filename = msFilenameStr;
            filename = filenamePrefix + filename;
            mkdirRecursive(getFilepath(filename));
            FILE* fout = nullptr;
            printf("open temporary file %s\n", filenameTemp.c_str());
            fout = fopen(filenameTemp.c_str(), "wb");
            if (!fout) {
                fprintf(stderr, "%s: %s\n", filenameTemp.c_str(), strerror(errno));
                exit(EXIT_FAILURE);
            }
            fseek(fp, offset, SEEK_SET);
            unsigned long long byteRead = 0;
            unsigned char fileBuffer[16384];
            while (byteRead < fileCompressedSize) {
                unsigned long long n;
                n = fread(fileBuffer, sizeof(unsigned char), 16384, fp);
                fwrite(fileBuffer, sizeof(unsigned char), n, fout);
                byteRead += n;
            }
            fclose(fout);
            // if compressed, decompress it
            bool decompressFlag = false;
            std::string filenameTempDecompress = filenameTemp + "_decompress";
            if (segment[0] || segment[1] || segment[2] || segment[3]) {
                decompressFlag = true;
                printf("decompress file\n");
                FILE* decFin = fopen(filenameTemp.c_str(), "rb");
                if (!decFin) {
                    fprintf(stderr, "%s: %s\n", filenameTemp.c_str(), strerror(errno));
                    exit(EXIT_FAILURE);
                }
                FILE *decFout = fopen(filenameTempDecompress.c_str(), "wb");
                if (!decFout) {
                    fprintf(stderr, "%s: %s\n", filenameTempDecompress.c_str(), strerror(errno));
                    exit(EXIT_FAILURE);
                }
                int result = funcompress(decFin, decFout);
                if (result != Z_OK) {
                    zerr(result);
                    exit(EXIT_FAILURE);
                }
                fclose(decFin);
                fclose(decFout);
            }
            FILE* uncFin = nullptr;
            if (decompressFlag) {
                uncFin = fopen(filenameTempDecompress.c_str(), "rb");
                if (!uncFin) {
                    fprintf(stderr, "%s: %s\n", filenameTempDecompress.c_str(), strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            else {
                uncFin = fopen(filenameTemp.c_str(), "rb");
                if (!uncFin) {
                    fprintf(stderr, "%s: %s\n", filenameTemp.c_str(), strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            printf("open output file %s\n", filename.c_str());
            FILE* uncFout;
            if (!i) {
                uncFout = fopen(filename.c_str(), "wb");
            }
            else{
                uncFout = fopen(filename.c_str(), "ab");
            }
            if (!uncFout) {
                fprintf(stderr, "%s: %s\n", filename.c_str(), strerror(errno));
                exit(EXIT_FAILURE);
            }
            unsigned long long byteWrite = 0;
            unsigned char writeBuffer[16384];
            while (byteWrite < fileSizeOriginal) {
                unsigned long long n;
                n = fread(writeBuffer, sizeof(unsigned char), 16384, uncFin);
                fwrite(writeBuffer, sizeof(unsigned char), n, uncFout);
                byteWrite += n;
            }
            fclose(uncFin);
            fclose(uncFout);
            if (decompressFlag) {
                printf("remove temporary file %s\n", filenameTempDecompress.c_str());
                int rremoveRet = remove(filenameTempDecompress.c_str());
                if (rremoveRet < 0) {
                    fprintf(stderr, "%s: %s\n", filenameTempDecompress.c_str(), strerror(errno));
                    errorFileList.push_back(filenameTempDecompress);
                }
            }
            printf("remove temporary file %s\n", filenameTemp.c_str());
            int removeRet = remove(filenameTemp.c_str());
            if (removeRet < 0) {
                fprintf(stderr, "%s: %s\n", filenameTemp.c_str(), strerror(errno));
                errorFileList.push_back(filenameTemp);
            }
            delete[] msFilenameStr;
        }
        printf("%ls: finished\n\n", fileNameStr);
        // adlr, not implemented
        fileInfoIdx += 4; // "adlr"
        fileInfoIdx += 8;
        fileInfoIdx += 4;
        delete[] fileNameStr;
        delete[] segment;
    }
    fclose(fp);
    delete[] fileInfo;
    if (errorFileList.empty()) {
        printf("error: %llu\n", errorFileList.size());
    }
    else{
        printf("error: %llu\n", errorFileList.size());
        for (auto i : errorFileList) {
            printf("%s\n", i.c_str());
        }
    }
    printf("\n");
    return 0;
}

// return -2: error, -1: no permission 0: don't exist, 1: regular file, 2: directory, 3: other
int isExist(const std::string& filePath) {
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return 0;
        }
        else if (errno == EACCES) {
            return -1;
        }
        else {
            return -2;
        }
    }
    if (S_ISREG(st.st_mode)) {
        return 1;
    }
    else if (S_ISDIR(st.st_mode)) {
        return 2;
    }
    else {
        return 3;
    }
}

int mkdirRecursive(const string& path) {
    if (isExist(path) == 2) {
        return 0;
    }
    std::string temp = path;
    if (temp.back() != '/') {
        temp += "/";
    }
    char pathc[256];
    strncpy(pathc, temp.c_str(), 256);
    if (pathc[0] == '/') {
        fprintf(stderr, "%s: absolute path, rejected\n", pathc);
        exit(EXIT_FAILURE);
    }
    for (char* ptr = pathc; *ptr; ++ptr) {
        if (*ptr == '/') {
            *ptr = '\0';
            printf("create directory %s\n", pathc);
            int mkdirRet = _mkdir(pathc);
            if (mkdirRet < 0) {
                if (errno != EEXIST) {
                    fprintf(stderr, "%s: %s\n", pathc, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            *ptr = '/';
        }
    }
    return 0;
}

bool is230later(const unsigned char* buffer) {
    return buffer[0] == 0x17 &&
           !buffer[1] &&
           !buffer[2] &&
           !buffer[3] &&
           !buffer[4] &&
           !buffer[5] &&
           !buffer[6] &&
           !buffer[7];
}

void aread(unsigned char* dst, const unsigned char* src, unsigned long long& idx, const unsigned long long& size) {
    for (unsigned long long i = 0; i < size; ++i) {
        dst[i] = src[idx + i];
    }
    idx += size;
}

void readUnsignedLongLong(const unsigned char* src, unsigned long long& idx, unsigned long long& dst) {
    dst = 0;
    for (int i = 0; i < 8; ++i) {
        dst |= (src[idx++] & (0xFFu)) << (i * 8);
    }
}

void zlibUncompress(unsigned char* dst, unsigned long long dstLen, unsigned char* src, unsigned long long srcLen) {
    uLong dstlen, srclen;
    srclen = static_cast<uLong>(srcLen);
    dstlen = static_cast<uLong>(dstLen);
    int result = uncompress(dst, &dstlen, src, srclen);
    if (result != Z_OK) {
        fprintf(stderr, "zlib: %s\n", zError(result));
        exit(EXIT_FAILURE);
    }
}

void replacePathDelimiter(std::string& src) {
    for (unsigned long long i = 0; i < src.length(); ++i) {
        if (src[i] == '\\') {
            src[i] = '/';
        }
    }
}

std::string getFilepath(const std::string& filename) {
    unsigned long long loc = filename.find_last_of("/");
    if (loc == std::string::npos) {
        return filename;
    }
    else {
        return filename.substr(0, loc);
    }
}

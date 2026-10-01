#include <cest>
#include <cstdio>
#include <string>
#include <sys/stat.h>
extern "C" {
#include <file_io.h>
}

static std::string written_path;
static size_t written_size = 0;

static int recordWrite(const char *path, const uint8_t *, size_t size) {
  written_path = path;
  written_size = size;
  return 0;
}

static bool isDirectory(const std::string &path) {
  struct stat status;
  return stat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

describe("FileIo", []() {
  afterEach([]() {
    FileIo_SetWriteBytesFunction(NULL);
  });

  it("routes writes through an installed stub", []() {
    const uint8_t data[] = {1, 2, 3};
    FileIo_SetWriteBytesFunction(recordWrite);
    expect(FileIo_WriteBytes("stub.glb", data, 3)).toBe(0);
    expect(written_path).toBe("stub.glb");
    expect(written_size).toBe((size_t)3);
  });

  it("writes real files by default", []() {
    const uint8_t data[] = {9, 8};
    std::string path = std::string(TEST_OUTPUT_DIR) + "/file-io.bin";
    expect(FileIo_WriteBytes(path.c_str(), data, 2)).toBe(0);
    FILE *file = fopen(path.c_str(), "rb");
    expect(file).toBeNotNull();
    expect(fgetc(file)).toBe(9);
    fclose(file);
    remove(path.c_str());
  });

  it("fails to write into a missing directory", []() {
    const uint8_t data[] = {0};
    expect(FileIo_WriteBytes(TEST_OUTPUT_DIR "/missing/dir/x.bin", data, 1)).Not->toBe(0);
  });

  it("creates nested directories and accepts existing ones", []() {
    std::string base = std::string(TEST_OUTPUT_DIR) + "/nested";
    std::string deep = base + "/a/b";
    expect((int)FileIo_EnsureDirectory(deep.c_str())).toBe((int)ERR_NONE);
    expect(isDirectory(deep)).toBeTruthy();
    expect((int)FileIo_EnsureDirectory(deep.c_str())).toBe((int)ERR_NONE);
    rmdir(deep.c_str());
    rmdir((base + "/a").c_str());
    rmdir(base.c_str());
  });

  it("reports a file in the way as ERR_WRITE_OUTPUT", []() {
    std::string blocker = std::string(TEST_OUTPUT_DIR) + "/blocker";
    FILE *file = fopen(blocker.c_str(), "w");
    fclose(file);
    expect((int)FileIo_EnsureDirectory((blocker + "/sub").c_str())).toBe((int)ERR_WRITE_OUTPUT);
    remove(blocker.c_str());
  });
});

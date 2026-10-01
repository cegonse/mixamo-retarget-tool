#include <cest>
#include <cstring>
extern "C" {
#include <glb_container.h>
}

static ByteBuffer *bin = nullptr;
static ByteBuffer *framed = nullptr;

static uint32_t readUint32(const uint8_t *data) {
  return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

describe("GlbContainer", []() {
  beforeEach([]() {
    ErrorCode error;
    bin = ByteBuffer_Create();
    ByteBuffer_AppendByte(bin, 0xAB);
    framed = GlbContainer_Frame("{\"a\":1}", 7, bin, &error);
    expect((int)error).toBe((int)ERR_NONE);
  });

  afterEach([]() {
    ByteBuffer_Destroy(framed);
    ByteBuffer_Destroy(bin);
    framed = bin = nullptr;
  });

  it("writes the 12-byte header with the total length", []() {
    const uint8_t *data = ByteBuffer_Data(framed);
    expect(readUint32(data)).toBe((uint32_t)GLB_MAGIC);
    expect(readUint32(data + 4)).toBe((uint32_t)2);
    expect(readUint32(data + 8)).toBe((uint32_t)ByteBuffer_Size(framed));
    expect(ByteBuffer_Size(framed)).toBe((size_t)(12 + 8 + 8 + 8 + 4));
  });

  it("pads the JSON chunk with spaces", []() {
    const uint8_t *data = ByteBuffer_Data(framed);
    expect(readUint32(data + 12)).toBe((uint32_t)8);
    expect(readUint32(data + 16)).toBe((uint32_t)GLB_CHUNK_JSON);
    expect(memcmp(data + 20, "{\"a\":1} ", 8)).toBe(0);
  });

  it("pads the BIN chunk with zeros", []() {
    const uint8_t *data = ByteBuffer_Data(framed);
    const unsigned char expected[] = {0xAB, 0x00, 0x00, 0x00};
    expect(readUint32(data + 28)).toBe((uint32_t)4);
    expect(readUint32(data + 32)).toBe((uint32_t)GLB_CHUNK_BIN);
    expect((const unsigned char *)data + 36).toEqualMemory(expected, 4);
  });
});

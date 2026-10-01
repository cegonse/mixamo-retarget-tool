#include <cest>
extern "C" {
#include <byte_buffer.h>
}

static ByteBuffer *buffer = nullptr;

describe("ByteBuffer", []() {
  beforeEach([]() {
    buffer = ByteBuffer_Create();
  });

  afterEach([]() {
    ByteBuffer_Destroy(buffer);
    buffer = nullptr;
  });

  it("starts empty", []() {
    expect(ByteBuffer_Size(buffer)).toBe((size_t)0);
  });

  it("writes uint32 little-endian", []() {
    const unsigned char expected[] = {0x67, 0x6C, 0x54, 0x46};
    ByteBuffer_AppendUint32(buffer, 0x46546C67u);
    expect((const unsigned char *)ByteBuffer_Data(buffer)).toEqualMemory(expected, 4);
  });

  it("writes float32 little-endian", []() {
    const unsigned char expected[] = {0x00, 0x00, 0x80, 0x3F};
    ByteBuffer_AppendFloat(buffer, 1.0f);
    expect((const unsigned char *)ByteBuffer_Data(buffer)).toEqualMemory(expected, 4);
  });

  it("pads to a multiple of four with the given byte", []() {
    const unsigned char expected[] = {0x01, 0x20, 0x20, 0x20};
    ByteBuffer_AppendByte(buffer, 0x01);
    ByteBuffer_PadTo4(buffer, 0x20);
    expect(ByteBuffer_Size(buffer)).toBe((size_t)4);
    expect((const unsigned char *)ByteBuffer_Data(buffer)).toEqualMemory(expected, 4);
    ByteBuffer_PadTo4(buffer, 0x20);
    expect(ByteBuffer_Size(buffer)).toBe((size_t)4);
  });

  it("grows past its initial capacity", []() {
    for (int index = 0; index < 1000; index++) {
      ByteBuffer_AppendUint32(buffer, (uint32_t)index);
    }
    expect(ByteBuffer_Size(buffer)).toBe((size_t)4000);
    expect(ByteBuffer_Data(buffer)[3996]).toBe((uint8_t)(999 & 0xFF));
  });

  it("destroys NULL harmlessly", []() {
    ByteBuffer_Destroy(NULL);
  });
});

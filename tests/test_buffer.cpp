#include "bt-embedded/buffer.h"

#include <gtest/gtest.h>

TEST(Buffer, testAlloc)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    ASSERT_EQ(buffer->total_size, 30);
    ASSERT_EQ(buffer->size, 10);
    ASSERT_EQ(buffer->next->size, 10);
    ASSERT_EQ(buffer->next->next->size, 10);
    ASSERT_EQ(buffer->next->next->next, nullptr);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testWriterSegmented)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    const std::string text = "A string between 20 and 30"; /* 26 */

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);
    bool ok = bte_buffer_writer_write(&writer, text.substr(0, 15).c_str(), 15);
    ASSERT_TRUE(ok);
    ok = bte_buffer_writer_write(&writer, text.substr(15).c_str(),
                                 text.size() - 15);
    ASSERT_TRUE(ok);
    bte_buffer_writer_end(&writer);

    std::string first((char*)buffer->data, 10);
    ASSERT_EQ(first, (text.substr(0, 10)));

    std::string second((char*)buffer->next->data, 10);
    ASSERT_EQ(second, (text.substr(10, 10)));

    std::string third((char*)buffer->next->next->data, buffer->next->next->size);
    ASSERT_EQ(third, (text.substr(20)));

    bte_buffer_unref(buffer);
}

TEST(Buffer, testWriterZeroCopyMax)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);
    uint16_t size = 0;
    char *ptr = (char *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 10);

    /* Pretend that we did write something */
    bte_buffer_writer_advance(&writer, size);

    ptr = (char *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 10);

    /* Pretend that we did write 6 bytes only */
    bte_buffer_writer_advance(&writer, 6);

    /* Remainder should be 4 */
    ptr = (char *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 4);
    bte_buffer_writer_advance(&writer, size);

    ptr = (char *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 10);
    bte_buffer_writer_advance(&writer, size);

    /* No space left */
    ptr = (char *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(size, 0);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testWriterZeroCopyFixed)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);

    /* Ask for too much */
    char *ptr = (char *)bte_buffer_writer_ptr_n(&writer, 11);
    ASSERT_TRUE(ptr == nullptr);

    /* Ask for the whole buffer */
    ptr = (char *)bte_buffer_writer_ptr_n(&writer, 10);
    ASSERT_TRUE(ptr != nullptr);

    /* Ask for a single byte */
    ptr = (char *)bte_buffer_writer_ptr_n(&writer, 1);
    ASSERT_TRUE(ptr != nullptr);

    /* Then for 10 more (should fail, only 9 remain) */
    ptr = (char *)bte_buffer_writer_ptr_n(&writer, 10);
    ASSERT_TRUE(ptr == nullptr);

    ptr = (char *)bte_buffer_writer_ptr_n(&writer, 9);
    ASSERT_TRUE(ptr != nullptr);

    /* Finish the writing, check the total size */
    bte_buffer_writer_end(&writer);
    ASSERT_EQ(buffer->total_size, 20);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testReaderSegmented)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    const std::string text = "A string between 20 and 30"; /* 26 */
    buffer->total_size = text.size();
    memcpy(buffer->data, text.c_str(), 10);
    memcpy(buffer->next->data, text.c_str() + 10, 10);
    memcpy(buffer->next->next->data, text.c_str() + 20, 6);
    buffer->next->next->size = 6;

    BteBufferReader reader;
    bte_buffer_reader_init(&reader, buffer);
    char dest[50];
    memset(dest, 0, sizeof(dest));
    uint16_t read = bte_buffer_reader_read(&reader, dest, sizeof(dest));
    ASSERT_EQ(read, text.size());
    ASSERT_EQ(std::string(dest), text);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testReaderZeroCopySegmented)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    const std::string text = "A string between 20 and 30"; /* 26 */
    buffer->total_size = text.size();
    memcpy(buffer->data, text.c_str(), 10);
    memcpy(buffer->next->data, text.c_str() + 10, 10);
    memcpy(buffer->next->next->data, text.c_str() + 20, 6);
    buffer->next->next->size = 6;

    BteBufferReader reader;
    bte_buffer_reader_init(&reader, buffer);
    uint16_t size = 0;
    const char *ptr = (const char *)bte_buffer_reader_read_max(&reader, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 10);
    ASSERT_EQ(std::string(ptr, 10), (text.substr(0, 10)));

    ptr = (const char *)bte_buffer_reader_read_max(&reader, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 10);
    ASSERT_EQ(std::string(ptr, 10), (text.substr(10, 10)));

    ptr = (const char *)bte_buffer_reader_read_max(&reader, &size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(size, 6);
    ASSERT_EQ(std::string(ptr, 6), (text.substr(20, 6)));

    ptr = (const char *)bte_buffer_reader_read_max(&reader, &size);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(size, 0);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testReaderZeroCopyRequired)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    const std::string text = "A string between 20 and 30"; /* 26 */
    buffer->total_size = text.size();
    memcpy(buffer->data, text.c_str(), 10);
    memcpy(buffer->next->data, text.c_str() + 10, 10);
    memcpy(buffer->next->next->data, text.c_str() + 20, 6);
    buffer->next->next->size = 6;

    BteBufferReader reader;
    bte_buffer_reader_init(&reader, buffer);
    const char *ptr = (const char *)bte_buffer_reader_read_n(&reader, 8);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(std::string(ptr, 8), std::string("A string"));

    /* We have only two bytes left; this should fail */
    ptr = (const char *)bte_buffer_reader_read_n(&reader, 3);
    ASSERT_TRUE(ptr == nullptr);

    ptr = (const char *)bte_buffer_reader_read_n(&reader, 2);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(std::string(ptr, 2), std::string(" b"));

    ptr = (const char *)bte_buffer_reader_read_n(&reader, 10);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(std::string(ptr, 10), std::string("etween 20 "));

    ptr = (const char *)bte_buffer_reader_read_n(&reader, 4);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(std::string(ptr, 4), std::string("and "));

    ptr = (const char *)bte_buffer_reader_read_n(&reader, 2);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(std::string(ptr, 2), std::string("30"));

    /* There's no more data */
    ptr = (const char *)bte_buffer_reader_read_n(&reader, 1);
    ASSERT_TRUE(ptr == nullptr);

    bte_buffer_unref(buffer);
}

TEST(Buffer, testWriterHeaderSize)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    //                        12345678901234567890
    const std::string text = "A string to be split"; // 20

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);
    /* Write the headers */
    bte_buffer_writer_set_header_size(&writer, 2);
    memcpy(buffer->data, "01", 2);
    memcpy(buffer->next->data, "02", 2);
    memcpy(buffer->next->next->data, "03", 2);

    uint16_t size = 0;
    uint8_t *ptr = (uint8_t *)bte_buffer_writer_ptr_max(&writer, &size);
    ASSERT_EQ(ptr, buffer->data + 2);
    ASSERT_EQ(size, 8);

    bool ok = bte_buffer_writer_write(&writer, text.c_str(), text.size());
    ASSERT_TRUE(ok);
    bte_buffer_writer_end(&writer);

    std::string first((char*)buffer->data, 10);
    ASSERT_EQ(first, std::string("01A string"));

    std::string second((char*)buffer->next->data, 10);
    ASSERT_EQ(second, std::string("02 to be s"));

    std::string third((char*)buffer->next->next->data, 6);
    ASSERT_EQ(third, std::string("03plit"));

    ASSERT_EQ(buffer->total_size, 26);
    ASSERT_EQ(buffer->size, 10);
    ASSERT_EQ(buffer->next->size, 10);
    ASSERT_EQ(buffer->next->next->size, 6);
    bte_buffer_unref(buffer);
}

TEST(Buffer, testReaderHeaderSize)
{
    BteBuffer *buffer = bte_buffer_alloc(30, 10);
    ASSERT_TRUE(buffer != nullptr);

    //                        12345678901234567890123456
    const std::string text = "01A string02 to be s03plit"; // 26

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);
    bool ok = bte_buffer_writer_write(&writer, text.c_str(), text.size());
    ASSERT_TRUE(ok);
    bte_buffer_writer_end(&writer);

    BteBufferReader reader;
    bte_buffer_reader_init(&reader, buffer);
    bte_buffer_reader_set_header_size(&reader, 2);

    char dest[50];
    memset(dest, 0, sizeof(dest));
    uint16_t read = bte_buffer_reader_read(&reader, dest, sizeof(dest));
    ASSERT_EQ(read, 20);

    std::string received(dest, read);
    ASSERT_EQ(received, std::string("A string to be split"));

    /* Test the zerocopy API */
    bte_buffer_reader_init(&reader, buffer);
    bte_buffer_reader_set_header_size(&reader, 2);

    uint16_t size = 0;
    const uint8_t *ptr =
        (const uint8_t *)bte_buffer_reader_read_max(&reader, &size);
    ASSERT_EQ(ptr, buffer->data + 2);
    ASSERT_EQ(size, 8);

    /* One byte too many */
    ptr = (const uint8_t *)bte_buffer_reader_read_n(&reader, 9);
    ASSERT_TRUE(ptr == nullptr);

    /* Read full buffer */
    ptr = (const uint8_t *)bte_buffer_reader_read_n(&reader, 8);
    ASSERT_EQ(ptr, buffer->next->data + 2);

    /* Read some more */
    ptr = (const uint8_t *)bte_buffer_reader_read_n(&reader, 2);
    ASSERT_EQ(ptr, buffer->next->next->data + 2);

    /* One byte too many again */
    ptr = (const uint8_t *)bte_buffer_reader_read_n(&reader, 3);
    ASSERT_TRUE(ptr == nullptr);

    ptr = (const uint8_t *)bte_buffer_reader_read_n(&reader, 2);
    ASSERT_EQ(ptr, buffer->next->next->data + 4);
    bte_buffer_unref(buffer);
}

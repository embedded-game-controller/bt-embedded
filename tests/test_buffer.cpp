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

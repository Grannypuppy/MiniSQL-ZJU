#include "record/row.h"

/**
* TODO: Student Implement
*/
uint32_t Row::SerializeTo(char *buf, Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema!");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields in serialization count dismatch!");

  char *pos = buf;
  uint32_t field_count = schema->GetColumnCount();
  uint32_t bitmap_bytes_count = (field_count + 7) / 8; //向上取整

  // 写入字段数量
  MACH_WRITE_UINT32(pos, field_count);
  pos += sizeof(uint32_t);

  // 生成并写入null_bitmap:来记录哪些字段为空
  std::vector<uint8_t> null_bitmap(bitmap_bytes_count, 0);
  for (uint32_t i = 0; i < field_count; ++i) {
    if (fields_[i]->IsNull()) {
      null_bitmap[i / 8] |= (1 << (i % 8));
    }
  }
  memcpy(pos, null_bitmap.data(), bitmap_bytes_count);
  pos += bitmap_bytes_count;

  // 序列化
  for (uint32_t i = 0; i < field_count; ++i) {
    if (!fields_[i]->IsNull()) {
      uint32_t move = fields_[i]->SerializeTo(pos);
      pos += move;
    }
  }

  return pos - buf;
}

/**
* TODO: Student Implement
*/
uint32_t Row::DeserializeFrom(char *buf, Schema *schema) {
  ASSERT(schema != nullptr, "Invalid schema!");
  ASSERT(fields_.empty(), "In deserialization, row shouldn't be empty!");

  char *pos = buf;
  uint32_t field_count = MACH_READ_UINT32(pos);
  pos += sizeof(uint32_t);
  ASSERT(field_count == schema->GetColumnCount(), "Fields in deserialization count dismatch!");

  // 读取null_bitmap
  uint32_t bitmap_bytes_count = (field_count + 7) / 8; // 向上取整
  std::vector<uint8_t> null_bitmap(bitmap_bytes_count);
  memcpy(null_bitmap.data(), pos, bitmap_bytes_count);
  pos += bitmap_bytes_count;

  //反序列化字段
  for (uint32_t i = 0; i < field_count; ++i) {
    bool is_null = (null_bitmap[i / 8] >> (i % 8)) & 1; // 判断是否为空
    Field *field = nullptr;
    uint32_t move = Field::DeserializeFrom(pos, schema->GetColumn(i)->GetType(), &field, is_null); // 注意Filed::,调用的是Field类的静态函数
    pos += move;
    fields_.push_back(field);
  }

  return pos - buf;
}

/**
* TODO: Student Implement
*/
uint32_t Row::GetSerializedSize(Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema!");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fileds in GetSerializedSize count dismatch!");

  uint32_t size = 0;
  uint32_t field_count = schema->GetColumnCount();
  uint32_t bitmap_bytes_count = (field_count + 7) / 8;

  size += sizeof(uint32_t); 
  size += bitmap_bytes_count; 

  for (uint32_t i = 0; i < field_count; ++i) {
    if (!fields_[i]->IsNull()) {
      size += fields_[i]->GetSerializedSize();
    }
  }

  return size;
}

/**
* TODO: Student Implement
*/
void Row::GetKeyFromRow(const Schema *source_schema, const Schema *key_schema, Row &key_row) {
  // 只提取key的字段，source_schema -> key_schema
  auto key_columns = key_schema->GetColumns();
  std::vector<Field> fields;
  uint32_t idx;

  for (const auto& column : key_columns) {
    source_schema->GetColumnIndex(column->GetName(), idx);
    fields.emplace_back(*this->GetField(idx));
  }
  key_row = Row(fields);
}
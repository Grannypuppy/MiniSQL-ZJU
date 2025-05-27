#include "record/row.h"

/**
* TODO: Student Implement
*/
uint32_t Row::SerializeTo(char *buf, Schema *schema) const {
  ASSERT(schema != nullptr, "INVALID SCHEMA");
  ASSERT(schema->GetColumnCount() == fields_.size(), "FILED COUNT MISMATCH");

  char *pos = buf;
  uint32_t field_count = schema->GetColumnCount();
  uint32_t bitmap_bytes_count = (field_count + 7) / 8; //向上取整

  // 写入字段数量
  MACH_WRITE_UINT32(pos, field_count);
  pos += sizeof(uint32_t);

  // 生成并写入 null bitmap
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
  ASSERT(schema != nullptr, "INVALID SCHEMA");
  ASSERT(fields_.empty(), "ROW SHOULD BE EMPTY BEFORE DESERIALIZATION");

  char *pos = buf;
  uint32_t field_count = MACH_READ_UINT32(pos);
  pos += sizeof(uint32_t);
  ASSERT(field_count == schema->GetColumnCount(), "DESERIALIZATION FIELD COUNT MISMATCH WITH SCHEMA");

  uint32_t bitmap_bytes_count = (field_count + 7) / 8;
  std::vector<uint8_t> null_bitmap(bitmap_bytes_count);
  memcpy(null_bitmap.data(), pos, bitmap_bytes_count);
  pos += bitmap_bytes_count;

  //反序列化字段
  for (uint32_t i = 0; i < field_count; ++i) {
    bool is_null = (null_bitmap[i / 8] >> (i % 8)) & 1;
    Field *field = nullptr;
    uint32_t move = Field::DeserializeFrom(pos, schema->GetColumn(i)->GetType(), &field, is_null);
    pos += move;
    fields_.push_back(field);
  }

  return pos - buf;
}

/**
* TODO: Student Implement
*/
uint32_t Row::GetSerializedSize(Schema *schema) const {
  ASSERT(schema != nullptr, "INVALID SCHEMA PROVIDED FOR SIZE CALCULATION.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "FIELD COUNT MISMATCH WITH SCHEMA COLUMN COUNT.");

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
  auto key_columns = key_schema->GetColumns();
  std::vector<Field> fields;
  uint32_t idx;

  for (const auto& column : key_columns) {
    source_schema->GetColumnIndex(column->GetName(), idx);
    fields.emplace_back(*this->GetField(idx));
  }
  key_row = Row(fields);
}
#include "record/column.h"

#include "glog/logging.h"

Column::Column(std::string column_name, TypeId type, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)), type_(type), table_ind_(index), nullable_(nullable), unique_(unique) {
  ASSERT(type != TypeId::kTypeChar, "Wrong constructor for CHAR type.");
  switch (type) {
    case TypeId::kTypeInt:
      len_ = sizeof(int32_t);
      break;
    case TypeId::kTypeFloat:
      len_ = sizeof(float_t);
      break;
    default:
      ASSERT(false, "Unsupported column type.");
  }
}

Column::Column(std::string column_name, TypeId type, uint32_t length, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)),
      type_(type),
      len_(length),
      table_ind_(index),
      nullable_(nullable),
      unique_(unique) {
  ASSERT(type == TypeId::kTypeChar, "Wrong constructor for non-VARCHAR type.");
}

Column::Column(const Column *other)
    : name_(other->name_),
      type_(other->type_),
      len_(other->len_),
      table_ind_(other->table_ind_),
      nullable_(other->nullable_),
      unique_(other->unique_) {}

/**
* TODO: Student Implement
*/
uint32_t Column::SerializeTo(char *buf) const {
    //写入魔数
    char *pos = buf;
    MACH_WRITE_UINT32(pos, Column::COLUMN_MAGIC_NUM);
    pos += sizeof(uint32_t);

    //写入列名（先长度再内容）
    MACH_WRITE_UINT32(pos, static_cast<uint32_t>(name_.size()));
    pos += sizeof(uint32_t);
    MACH_WRITE_STRING(pos, name_);
    pos += name_.size();

    //写入类型
    MACH_WRITE_UINT32(pos, static_cast<uint32_t>(type_));
    pos += sizeof(uint32_t);

    //写入长度（对于 CHAR 即 varchar 长度，否则是固定大小）
    MACH_WRITE_UINT32(pos, len_);
    pos += sizeof(uint32_t);

    //写入表内索引
    MACH_WRITE_UINT32(pos, table_ind_);
    pos += sizeof(uint32_t);

    //写入可空标志和唯一标志（各占 1 字节）
    *pos = static_cast<char>(nullable_);
    pos += sizeof(char);
    *pos = static_cast<char>(unique_);
    pos += sizeof(char);

    return pos - buf;
}

/**
 * TODO: Student Implement
 */
uint32_t Column::GetSerializedSize() const {
    return
        sizeof(uint32_t)                              // magic
        + sizeof(uint32_t) + name_.size()             // 列名长度 + 列名内容
        + sizeof(uint32_t)                            // type_
        + sizeof(uint32_t)                            // len_
        + sizeof(uint32_t)                            // table_ind_
        + sizeof(char)                                // nullable_
        + sizeof(char);                               // unique_
}

/**
 * TODO: Student Implement
 */
uint32_t Column::DeserializeFrom(char *buf, Column *&column) {
    //读魔数并校验
    char *pos = buf;
    uint32_t magic = MACH_READ_UINT32(pos);
    ASSERT(magic == Column::COLUMN_MAGIC_NUM, "Column DeserializeFrom wrong magic number.");
    pos += sizeof(uint32_t);

    //读列名
    uint32_t name_len = MACH_READ_UINT32(pos);
    pos += sizeof(uint32_t);
    std::string name(pos, name_len);
    pos += name_len;

    //读类型
    TypeId type = static_cast<TypeId>(MACH_READ_UINT32(pos));
    pos += sizeof(uint32_t);

    //读长度
    uint32_t len = MACH_READ_UINT32(pos);
    pos += sizeof(uint32_t);

    //读表内索引
    uint32_t table_ind = MACH_READ_UINT32(pos);
    pos += sizeof(uint32_t);

    //读可空和唯一标志
    bool nullable = static_cast<bool>(*pos);
    pos += sizeof(char);
    bool unique = static_cast<bool>(*pos);
    pos += sizeof(char);

    //构造
    if (type == TypeId::kTypeChar) {
      column = new Column(name, type, len, table_ind, nullable, unique);
    } else {
      column = new Column(name, type, table_ind, nullable, unique);
    }

    return pos - buf;
}
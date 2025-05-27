#include "catalog/catalog.h"

void CatalogMeta::SerializeTo(char *buf) const {
  ASSERT(GetSerializedSize() <= PAGE_SIZE, "Failed to serialize catalog metadata to disk.");
  MACH_WRITE_UINT32(buf, CATALOG_METADATA_MAGIC_NUM);
  buf += 4;
  MACH_WRITE_UINT32(buf, table_meta_pages_.size());
  buf += 4;
  MACH_WRITE_UINT32(buf, index_meta_pages_.size());
  buf += 4;
  for (auto iter : table_meta_pages_) {
    MACH_WRITE_TO(table_id_t, buf, iter.first);
    buf += 4;
    MACH_WRITE_TO(page_id_t, buf, iter.second);
    buf += 4;
  }
  for (auto iter : index_meta_pages_) {
    MACH_WRITE_TO(index_id_t, buf, iter.first);
    buf += 4;
    MACH_WRITE_TO(page_id_t, buf, iter.second);
    buf += 4;
  }
}

CatalogMeta *CatalogMeta::DeserializeFrom(char *buf) {
  // check valid
  uint32_t magic_num = MACH_READ_UINT32(buf);
  buf += 4;
  ASSERT(magic_num == CATALOG_METADATA_MAGIC_NUM, "Failed to deserialize catalog metadata from disk.");
  // get table and index nums
  uint32_t table_nums = MACH_READ_UINT32(buf);
  buf += 4;
  uint32_t index_nums = MACH_READ_UINT32(buf);
  buf += 4;
  // create metadata and read value
  CatalogMeta *meta = new CatalogMeta();
  for (uint32_t i = 0; i < table_nums; i++) {
    auto table_id = MACH_READ_FROM(table_id_t, buf);
    buf += 4;
    auto table_heap_page_id = MACH_READ_FROM(page_id_t, buf);
    buf += 4;
    meta->table_meta_pages_.emplace(table_id, table_heap_page_id);
  }
  for (uint32_t i = 0; i < index_nums; i++) {
    auto index_id = MACH_READ_FROM(index_id_t, buf);
    buf += 4;
    auto index_page_id = MACH_READ_FROM(page_id_t, buf);
    buf += 4;
    meta->index_meta_pages_.emplace(index_id, index_page_id);
  }
  return meta;
}

/**
 * TODO: Student Implement
 */
uint32_t CatalogMeta::GetSerializedSize() const {
  return 4 + 4 + 4 + table_meta_pages_.size() * (4 + 4) + index_meta_pages_.size() * (4 + 4);
}

CatalogMeta::CatalogMeta() {}

/**
 * TODO: Student Implement
 */
CatalogManager::CatalogManager(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager,
                               LogManager *log_manager, bool init)
    : buffer_pool_manager_(buffer_pool_manager), lock_manager_(lock_manager), log_manager_(log_manager) {
  if (init) {
    // 新建数据库
    catalog_meta_ = CatalogMeta::NewInstance();
    next_table_id_ = 0;
    next_index_id_ = 0;
    // 将新的catalog_meta_写入磁盘
    FlushCatalogMetaPage();
  } else {
    // 从磁盘加载已有数据库
    Page *catalog_page = buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
    catalog_meta_ = CatalogMeta::DeserializeFrom(catalog_page->GetData());
    buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID, false);

    // 加载所有表和索引
    for (auto &table_meta : catalog_meta_->table_meta_pages_) {
      LoadTable(table_meta.first, table_meta.second);
    }
    for (auto &index_meta : catalog_meta_->index_meta_pages_) {
      LoadIndex(index_meta.first, index_meta.second);
    }

    // 设置下一个可用的ID
    next_table_id_ = catalog_meta_->GetNextTableId();
    next_index_id_ = catalog_meta_->GetNextIndexId();
  }
}

CatalogManager::~CatalogManager() {
  FlushCatalogMetaPage();
  delete catalog_meta_;
  for (auto iter : tables_) {
    delete iter.second;
  }
  for (auto iter : indexes_) {
    delete iter.second;
  }
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateTable(const string &table_name, TableSchema *schema, Txn *txn, TableInfo *&table_info) {
  // 检查表名是否已存在，防止重复创建
  if (table_names_.find(table_name) != table_names_.end()) {
    return DB_TABLE_ALREADY_EXIST;  // 若表名已存在，返回错误码
  }

  // 分配新的表ID，原子操作确保线程安全
  table_id_t table_id = next_table_id_.fetch_add(1);

  // 创建一个新页用于存储表的元数据
  page_id_t meta_page_id;  // 表元数据页的页面ID
  Page *meta_page = buffer_pool_manager_->NewPage(
      &meta_page_id);  // 创建新页面存储表元数据：NewPage会赋值一个NewPage给meta_page_id，并返回一个指向新页面的指针
  if (meta_page == nullptr) {
    return DB_FAILED;  // 如果无法创建新页面，返回失败
  }

  // 创建表堆，用于实际存储表的数据
  TableHeap *table_heap = TableHeap::Create(buffer_pool_manager_, schema, txn, log_manager_,
                                            lock_manager_);  // 这个Create函数会自动分配有一个root_page_id
  if (table_heap == nullptr) {
    // 如果表堆创建失败，释放之前分配的元数据页
    buffer_pool_manager_->DeletePage(meta_page_id);
    return DB_FAILED;
  }

  // 获取表堆的根页ID
  page_id_t root_page_id = table_heap->GetFirstPageId();

  // 创建表的元数据，包括表ID、表名、根页ID和表结构
  TableMetadata *table_meta = TableMetadata::Create(table_id, table_name, root_page_id, schema);
  if (table_meta == nullptr) {
    // 如果表元数据创建失败，释放之前分配的表堆和元数据页
    table_heap->DeleteTable();
    buffer_pool_manager_->DeletePage(meta_page_id);
    return DB_FAILED;
  }

  // 将表元数据序列化到元数据页中
  table_meta->SerializeTo(meta_page->GetData());

  // 标记元数据页为脏页，确保数据会被写回磁盘
  buffer_pool_manager_->UnpinPage(meta_page_id, true);

  // 创建表信息对象，用于内存中管理表
  table_info = TableInfo::Create();

  // 初始化表信息，关联元数据和表堆
  table_info->Init(table_meta, table_heap);

  // 更新内存中的表记录
  tables_[table_id] = table_info;       // 保存表ID到表信息的映射
  table_names_[table_name] = table_id;  // 保存表名到表ID的映射

  // 更新目录元数据的表记录
  catalog_meta_->table_meta_pages_[table_id] = meta_page_id;

  // 将更新后的目录元数据刷新到磁盘
  ASSERT(FlushCatalogMetaPage() == DB_SUCCESS, "Failed to flush catalog metadata to disk.");

  // 创建表成功
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTable(const string &table_name, TableInfo *&table_info) {
  auto iter = table_names_.find(table_name);
  if (iter == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  table_info = tables_[iter->second];
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTables(vector<TableInfo *> &tables) const {
  tables.clear();
  tables.reserve(tables_.size());
  for (const auto iter : tables_) {
    tables.push_back(iter.second);
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateIndex(const string &table_name, const string &index_name,
                                    const vector<string> &index_keys, Txn *txn, IndexInfo *&index_info,
                                    const string &index_type) {
  // 检查表是否存在
  TableInfo *table_info = nullptr;
  if (GetTable(table_name, table_info) != DB_SUCCESS) {
    return DB_TABLE_NOT_EXIST;
  }
  // 检查索引名是否已存在
  if (index_names_[table_name].find(index_name) != index_names_[table_name].end()) {
    return DB_INDEX_ALREADY_EXIST;
  }

  // 检查索引键是否有效，并生成 key_map_
  vector<uint32_t> key_map;
  auto schema = table_info->GetSchema();
  for (const auto &key : index_keys) {
    uint32_t col_index;
    if (schema->GetColumnIndex(key, col_index) != DB_SUCCESS) {
      return DB_COLUMN_NAME_NOT_EXIST;  // 列名不存在，返回错误
    }
    key_map.push_back(col_index);
  }

  // 步骤5: 分配新的索引ID
  index_id_t index_id = next_index_id_.fetch_add(1);  // 原子操作，获取唯一索引ID

  // 创建索引的元数据页
  page_id_t meta_page_id;
  Page *meta_page = buffer_pool_manager_->NewPage(&meta_page_id);
  if (meta_page == nullptr) return DB_FAILED;

  // 创建索引的元数据
  IndexMetadata *index_meta = IndexMetadata::Create(index_id, index_name, table_info->GetTableId(), key_map);

  // 创建索引
  Index *index = Index::Create(index_type, index_meta, schema);
  if (index == nullptr) {
    delete index_meta;
    buffer_pool_manager_->DeletePage(meta_page_id);
    return DB_FAILED;
  }

  // 序列化索引的元数据到页面
  index_meta->SerializeTo(meta_page->GetData());
  buffer_pool_manager_->UnpinPage(meta_page_id, true);

  // 创建索引信息对象
  index_info = IndexInfo::Create();
  index_info->Init(index_meta, table_info, buffer_pool_manager_);
  indexes_[index_id] = index_info;
  index_names_[table_name][index_name] = index_id;

  // 更新catalog元数据
  catalog_meta_->index_meta_pages_[index_id] = meta_page_id;
  FlushCatalogMetaPage();

  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetIndex(const string &table_name, const string &index_name, IndexInfo *&index_info) const {
  auto table_iter = index_names_.find(table_name);
  if (table_iter == index_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }

  auto index_iter = table_iter->second.find(index_name);
  if (index_iter == table_iter->second.end()) {
    return DB_INDEX_NOT_FOUND;
  }

  auto index_id = index_iter->second;
  index_info = indexes_.at(index_id);
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTableIndexes(const string &table_name, vector<IndexInfo *> &indexes) const {
  auto table_iter = index_names_.find(table_name);
  if (table_iter == index_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }

  indexes.clear();
  for (const auto &pair : table_iter->second) {
    indexes.push_back(indexes_.at(pair.second));
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::DropTable(const string &table_name) {
    // 检查表是否存在
    auto table_iter = table_names_.find(table_name);
    if (table_iter == table_names_.end()) {
        return DB_TABLE_NOT_EXIST;
    }
    
    table_id_t table_id = table_iter->second;
    TableInfo *table_info = tables_[table_id];
    
    // 1.删除相关的所有索引
    auto index_iter = index_names_.find(table_name);
    if (index_iter != index_names_.end()) {
        for (const auto &pair : index_iter->second) {
            index_id_t index_id = pair.second;
            // 删除索引元数据页
            buffer_pool_manager_->DeletePage(catalog_meta_->index_meta_pages_[index_id]);
            catalog_meta_->index_meta_pages_.erase(index_id);
            // 删除索引对象
            delete indexes_[index_id];
            indexes_.erase(index_id);
        }
        index_names_.erase(index_iter);
    }
    
    // 删除表的元数据页
    buffer_pool_manager_->DeletePage(catalog_meta_->table_meta_pages_[table_id]);
    catalog_meta_->table_meta_pages_.erase(table_id);
    
    // 删除表对象
    delete table_info;
    tables_.erase(table_id);
    table_names_.erase(table_name);
    
    // 更新catalog元数据
    FlushCatalogMetaPage();
    
    return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::DropIndex(const string &table_name, const string &index_name) {
    // 检查表和索引是否存在
    auto table_iter = index_names_.find(table_name);
    if (table_iter == index_names_.end()) {
        return DB_TABLE_NOT_EXIST;
    }
    
    auto index_iter = table_iter->second.find(index_name);
    if (index_iter == table_iter->second.end()) {
        return DB_INDEX_NOT_FOUND;
    }
    
    index_id_t index_id = index_iter->second;
    
    // 删除索引元数据页
    buffer_pool_manager_->DeletePage(catalog_meta_->index_meta_pages_[index_id]);
    catalog_meta_->index_meta_pages_.erase(index_id);
    
    // 删除索引对象
    delete indexes_[index_id];
    indexes_.erase(index_id);
    table_iter->second.erase(index_name);
    
    // 如果表没有其他索引了，删除表的索引映射
    if (table_iter->second.empty()) {
        index_names_.erase(table_name);
    }
    
    // 更新catalog元数据
    FlushCatalogMetaPage();
    
    return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::FlushCatalogMetaPage() const {
  // ASSERT(false, "Not Implemented yet");
  return DB_FAILED;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::LoadTable(const table_id_t table_id, const page_id_t page_id) {
  // 从buffer pool获取表的元数据页
  Page *table_page = buffer_pool_manager_->FetchPage(page_id);
  if (table_page == nullptr) return DB_FAILED;

  // 反序列化表的元数据
  TableMetadata *table_meta = nullptr;
  TableMetadata::DeserializeFrom(table_page->GetData(), table_meta);
  if (table_meta == nullptr) {
    buffer_pool_manager_->UnpinPage(page_id, false);  // 如果反序列化失败，释放页，并且不是脏页
    return DB_FAILED;
  }

  // 创建表堆
  TableHeap *table_heap = TableHeap::Create(buffer_pool_manager_, table_meta->GetFirstPageId(), table_meta->GetSchema(),
                                            log_manager_, lock_manager_);

  // 创建表信息对象
  TableInfo *table_info = new TableInfo(table_meta, table_heap);
  tables_[table_id] = table_info;
  table_names_[table_meta->GetTableName()] = table_id;

  buffer_pool_manager_->UnpinPage(page_id, false);
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::LoadIndex(const index_id_t index_id, const page_id_t page_id) {
  // 从buffer pool获取索引的元数据页
  Page *index_page = buffer_pool_manager_->FetchPage(page_id);
  if (index_page == nullptr) return DB_FAILED;  // 如果页面获取失败，返回错误

  // 反序列化索引的元数据
  IndexMetadata *index_meta = nullptr;
  IndexMetadata::DeserializeFrom(index_page->GetData(), index_meta);  // 从页面数据中反序列化索引元数据
  if (index_meta == nullptr) {
    buffer_pool_manager_->UnpinPage(page_id, false);  // 如果反序列化失败，释放页面
    return DB_FAILED;
  }

  // 获取对应的表信息
  TableInfo *table_info = nullptr;
  // 这里使用了表ID来查找对应的表信息
  if (GetTable(index_meta->GetTableId(), table_info) != DB_SUCCESS) {  // 尝试获取索引对应的表信息
    delete index_meta;                                                 // 如果获取表信息失败，释放索引元数据
    buffer_pool_manager_->UnpinPage(page_id, false);                   // 释放页面
    return DB_FAILED;
  }

  // 创建索引信息对象
  IndexInfo *index_info = IndexInfo::Create();  // 创建一个新的索引信息对象

  // 初始化索引信息对象
  // 这里使用了索引元数据、表信息和缓冲池管理器来初始化索引
  index_info->Init(index_meta, table_info, buffer_pool_manager_);

  // 将索引信息添加到索引映射表中
  indexes_[index_id] = index_info;  // 使用索引ID作为键存储索引信息

  // 将索引名称映射到索引ID
  // 这里利用表名和索引名的组合来查找索引ID
  std::string table_name = table_info->GetTableName();
  std::string index_name = index_meta->GetIndexName();

  // 如果当前表名不存在于索引名称映射表中，则添加一个新的映射
  if (index_names_.find(table_name) == index_names_.end()) {
    index_names_[table_name] = std::unordered_map<std::string, index_id_t>();
  }

  // 将索引名称映射到索引ID
  index_names_[table_name][index_name] = index_id;

  // 释放索引元数据页面
  buffer_pool_manager_->UnpinPage(page_id, false);  // 释放页面，不需要写回（没有修改）

  // 加载成功
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTable(const table_id_t table_id, TableInfo *&table_info) {
    auto iter = tables_.find(table_id);
    if (iter == tables_.end()) {
        return DB_TABLE_NOT_EXIST;
    }
    table_info = iter->second;
    return DB_SUCCESS;
}
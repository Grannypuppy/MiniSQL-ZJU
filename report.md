<div class="cover" style="page-break-after:always;font-family:方正公文仿宋;width:100%;height:100%;border:none;margin: 0 auto;text-align:center;">
    <div style="width:100%;margin: 0 auto;height:0;padding-bottom:10%;">
        </br>
        <img src="https://raw.githubusercontent.com/Keldos-Li/pictures/main/typora-latex-theme/ZJU-name.svg" alt="校名" style="width:60%;"/>
    </div>
    </br></br></br></br></br>
    <div style="width:60%;margin: 0 auto;height:0;padding-bottom:40%;">
        <img src="https://raw.githubusercontent.com/Keldos-Li/pictures/main/typora-latex-theme/ZJU-logo.svg" alt="校徽" style="width:60%;"/>
    </div>
<font size = 59, style="width:40%;font-weight:normal;text-align:center;font-family:华文仿宋"> 数据库系统实验报告 </font>
    </br>
    </br>
</br></br></br></br></br>
    <table style="border:none;text-align:center;width:72%;font-family:仿宋;font-size:14px; margin: 0 auto;">
    <tbody style="font-family:方正公文仿宋;font-size:12pt;">
        <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">题　　目</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">MiniSQL</td>     </tr>
        <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">授课教师</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">苗晓烨</td>     </tr>
         <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">助　　教</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">江林益/王伟杰</td>     </tr>
        <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">姓　　名</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">宋嘉民</td>     </tr>
        <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">学　　号</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">3230105644</td>     </tr>
         <tr style="font-weight:normal;"> 
            <td style="width:10%;text-align:right;">邮　　箱</td>
            <td style="width:2%">：</td> 
            <td style="width:100%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">zjuheadmaster@zju.edu.cn</td>     </tr>
         <tr style="font-weight:normal;"> 
            <td style="width:20%;text-align:right;">联系电话</td>
            <td style="width:2%">：</td> 
            <td style="width:40%;font-weight:normal;border-bottom: 1px solid;text-align:center;font-family:华文仿宋">13428807817</td>     </tr>
</tbody>              
</table>
</div>





<font size = 8> Contents </font>

[toc]

## CATALOG MANAGER

**Catalog Manager** 负责管理和维护数据库的所有模式信息，包括：

- 数据库中所有表的定义信息，包括表的名称、表中字段（列）数、主键、定义在该表上的索引。
- 表中每个字段的定义信息，包括字段类型、是否唯一等。
- 数据库中所有索引的定义，包括所属表、索引建立在那个字段上等。

这些模式信息在被创建、修改和删除后还应被持久化到数据库文件中。此外，Catalog Manager还需要为上层的执行器Executor提供公共接口以供执行器获取目录信息并生成执行计划。

### 目录元信息

#### CatalogMeta

**CatalogMeta** 类负责管理数据库中表和索引的元数据。以下是其结构和关键方法的概述：

- `table_meta_pages_`_：存储表 ID 和对应元数据页 ID 之间的映射关系的 map。_
- `index_meta_pages_`：存储索引 ID 和对应元数据页 ID 之间的映射关系的 map。
  方法。
- `SerializeTo(char *buf) const`：将 `CatalogMeta` 对象序列化为字符缓冲区
- `DeserializeFrom(char *buf)`：将字符缓冲区反序列化为 `CatalogMeta` 对象。
- `GetSerializedSize() const`：计算序列化后的 `CatalogMeta` 对象的大小。
- `GetNextTableId() const`：返回下一个可用的表 ID。
- `GetNextIndexId() const`：返回下一个可用的索引 ID。
- `NewInstance()`：创建 `CatalogMeta` 的新实例。
- `GetTableMetaPages()`：返回指向存储表元数据页面的映射的指针（用于测试）。
- `GetIndexMetaPages()`：返回指向存储索引元数据页面的映射的指针（用于测试）。
- `DeleteIndexMetaPage(BufferPoolManager *bpm, index_id_t index_id)`：删除与给定索引 ID 相关联的元数据页。

`DeleteIndexMetaPage(BufferPoolManager *bpm, index_id_t index_id)`：删除与给定索引 ID 相关联的元数据页。
该类与 **CatalogManager** 密切相关，**CatalogManager** 使用 **CatalogMeta** 来管理数据库系统中表和索引的元数据。它提供了维护数据库系统中表和索引目录的必要功能。

`GetSerializedSize()` 方法用于计算 **CatalogMeta** 对象序列化后的大小。根据给出的计算方法，该方法返回的值为：

- 一个固定大小的魔术数字（`uint32_t`，4 个字节）。两个 `std::map` 对象的大小：每个 `std::map` 包括键值对，每个键值对需要 8 个字节（4 个字节的键和 4 个字节的值）。因此，加上魔术数字，总共为 12 个字节。然后将每个 `std::map` 中的键值对数量乘以 8，得到的值表示这些键值对的总大小。这样就得到了**CatalogMeta** 对象序列化后的总大小。


```cpp
uint32_t CatalogMeta::GetSerializedSize() const {
  //magic_num + size * 2 + map(4, 4)*2
  return 12 +(table_meta_pages_.size() + index_meta_pages_.size()) * 8;
}
```

#### IndexMetadata

**IndexMetadata** 类用于表示索引的元数据，包括索引的 ID、名称、所属表的 ID 以及索引键的映射。以下是该类的关键结构和方法：

- index_id_：索引的唯一标识符。index_name_：索引的名称。

- table_i_：该索引所属表的唯一标识符。_

- _key_map_：索引键与元组键的映射。

  **方法：**

- `Create()`：静态方法，用于创建新的 `IndexMetadata` 实例。

  `SerializeTo(char *buf) const`：将 `IndexMetadata` 对象序列化为字符缓冲区。

  `GetSerializedSize() const`：计算序列化后的 `IndexMetadata` 对象的大小。

  `DeserializeFrom(char *buf, IndexMetadata *&index_meta)`：从字符缓冲区反序列化出一个 `IndexMetadata` 。

  `GetIndexName() const`：返回索引的名称。

  `GetTableId() const`：返回索引所属表的唯一标识符。

  `GetIndexColumnCount() const`：返回索引的列数。

  `GetKeyMapping() const`：返回索引键与元组键的映射。

  `GetIndexId() const`：返回索引的唯一标识符。

该类主要用于管理数据库中索引的元数据信息，为索引的创建、序列化和反序列化提供了必要的功能。

`GetSerializedSize()` 方法用于计算 **IndexMetadata** 对象序列化后的大小。根据给出的计算方法，该方法返回的值为索引名称长度加上键映射大小以及其他固定大小的总和。具体计算如下：

每个键映射使用 4 个字节（假设 `uint32_t` 类型）。索引名称的长度由 `index_name_.length()` 给出。加上其他固定大小的部分，其中包括键映射大小（4 个字节）、索引 ID（2 个字节）、表 ID（2 个字节）和魔术数字（2 个字节）。因此，将以上各项大小相加即可得到 `IndexMetadata` 对象序列化后的总大小。

```cpp
uint32_t IndexMetadata::GetSerializedSize() const {
  return 4 +                                      // MAGIC_NUM 的大小
         4 +                                      // index_id_ 的大小
         MACH_STR_SERIALIZED_SIZE(index_name_) +  // index_name_ 的大小
         4 +                                      // table_id_ 的大小
         4 +                                      // key count : key_map_ 数组大小字段的大小
         4 * key_map_.size();                     // key_map_ 数组大小 (每个uint32_t占4字节)
}
```

#### IndexInfo

`Init` 方法用于初始化 `IndexInfo` 对象。具体步骤如下：

1. **初始化元数据和表信息**：将传入的元数据和表信息指针赋值给类成员变量。
2. **映射索引键到键模式**：使用 `Schema::ShallowCopySchema` 方法根据表的模式和索引键的映射关系创建索引键模式。
3. **创建索引**：调用 `CreateIndex` 方法创建索引。`CreateIndex` 方法需要根据索引类型（例如 "bptree"）和缓冲池管理器来创建索引对象。

```cpp
void Init(IndexMetadata *meta_data, TableInfo *table_info, BufferPoolManager *buffer_pool_manager) {
    // Step1: init index metadata and table info
    meta_data_ = meta_data;  // 保存索引的元数据引用
    
    // Step2: mapping index key to key schema
    // 获取表的模式
    auto schema = table_info->GetSchema();
    // 获取要建立索引的列映射
    auto key_map = meta_data_->GetKeyMapping();
    
    // 创建索引键模式 - 从表模式中选择特定的列作为索引键
    key_schema_ = Schema::ShallowCopySchema(schema, key_map);
    
    // Step3: call CreateIndex to create the index
    // 根据索引类型创建实际的索引对象 (默认使用B+树索引)
    index_ = CreateIndex(buffer_pool_manager, "bptree");
    
    // 确保索引创建成功
    ASSERT(index_ != nullptr, "Failed to create index.");
  }
```

`IndexInfo` 类的主要职责是：初始化索引元数据。映射索引键到键模式。创建索引。提供一些索引相关的信息访问方法。

#### TableMetadata

**TableMetadata** 类用于存储表的元数据，包括表的ID、名称、根页面ID以及表的模式（通过指向 Schema 对象的指针来表示）。以下是对该类的分析：

- table_id_：表的唯一标识符，类型为 table_id_t。_
- _table_name_：表的名称，类型为 std::string。
- root_page_id_：表的根页面ID，用于指示存储表数据的位置，类型为 page_id_t。_
- _schema_：指向 Schema 对象的指针，表示表的模式。
- `Create`：用于创建新的 `TableMetadata` 对象，需要提供表的ID、名称、根页面ID以及表的模式。
- `DeserializeFrom`：用于从缓冲区中反序列化数据，创建 `TableMetadata` 对象。该方法通常用于从磁盘中读取表的元数据并恢复对象。

- `SerializeTo`：将对象序列化为字符数组。该方法通常用于将表的元数据写入磁盘。
- `GetSerializedSize`：计算对象序列化后的大小，包括固定大小的头部信息和动态大小的成员变量（如表名的长度和模式的大小）。
- `GetTableId`：获取表的ID。
- `GetTableName`：获取表的名称。
- `GetFirstPageId`：获取表的根页面ID。
- `GetSchema`：获取表的模式对象。

使用了友元类 TableInfo，允许 TableInfo 类访问 TableMetadata 类的私有成员。类中包含了一个固定的魔术数字，用于标识序列化的对象类型。由于可能需要在对象销毁时释放 Schema 对象的内存，因此在实际使用中可能需要实现析构函数，但在提供的代码中被注释掉了。总体而言，`TableMetadata` 类提供了对表元数据的封装和管理，为数据库系统的元数据管理提供了基本的功能支持。

`TableMetadata` 类中的 `GetSerializedSize()` 方法用于计算序列化后对象的大小。根据该方法的实现和类的成员变量，计算得到的大小如下：

- 一个固定大小的魔术数字（`uint32_t`，4 个字节）。一个 `table_id_t` 类型的表ID（4 个字节）。一个字符串类型的表名，其大小等于字符串长度加上一个结尾的 null 字符（字符串长度 + 1）。一个 `page_id_t` 类型的根页面ID（4 个字节）。一个指向 `Schema` 对象的指针（通常是指针的大小，例如 4 或 8 个字节，取决于操作系统和编译器）。根据上述计算，将这些部分的大小相加即可得到 `TableMetadata` 对象序列化后的总大小。


```cpp
uint32_t TableMetadata::GetSerializedSize() const {
  return 4 + 4 + MACH_STR_SERIALIZED_SIZE(table_name_) + 4 + schema_->GetSerializedSize();
}
```

### 表和索引的管理

#### CreateTable

功能描述：创建新表，分配所需资源，并将新表的元数据信息序列化到磁盘。检查表是否已存在。创建新的表信息实例、表模式的深拷贝、表堆、表元数据。将表元数据序列化到新页并刷新到磁盘。更新表名映射、表信息、表元数据页映射。返回成功或失败。

```cpp
dberr_t CatalogManager::CreateTable(const string &table_name, TableSchema *schema, Txn *txn, TableInfo *&table_info) {
  // 检查表名是否已存在，防止重复创建
  if (table_names_.find(table_name) != table_names_.end()) {
    return DB_TABLE_ALREADY_EXIST;  // 若表名已存在，返回错误码
  }
  // 分配新的表ID，原子操作确保线程安全
  table_id_t table_id = next_table_id_.fetch_add(1);
  // 创建一个新页用于存储表的元数据
  page_id_t meta_page_id;  // 表元数据页的页面ID
  Page *meta_page = buffer_pool_manager_->NewPage(meta_page_id);  // 创建新页面存储表元数据：NewPage会赋值一个NewPage给meta_page_id，并返回一个指向新页面的指针
  if (meta_page == nullptr) {
    return DB_FAILED;  // 如果无法创建新页面，返回失败
  }
  TableSchema *copied_schema = Schema::DeepCopySchema(schema);
  // 创建表堆，用于实际存储表的数据
  TableHeap *table_heap = TableHeap::Create(buffer_pool_manager_, copied_schema, txn, log_manager_,lock_manager_);  // 这个Create函数会自动分配有一个root_page_id
  if (table_heap == nullptr) {
    // 如果表堆创建失败，释放之前分配的元数据页
    buffer_pool_manager_->DeletePage(meta_page_id);
    return DB_FAILED;
  }
  // 获取表堆的根页ID
  page_id_t root_page_id = table_heap->GetFirstPageId();
  // 创建表的元数据，包括表ID、表名、根页ID和表结构
  TableMetadata *table_meta = TableMetadata::Create(table_id, table_name, root_page_id, copied_schema);
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
```



#### GetTable

获取指定名称的表信息。根据表名查找表的 ID。使用表的 ID 查找表信息，并存储到输出参数中。返回成功或失败。

```cpp
dberr_t CatalogManager::GetTable(const string &table_name, TableInfo *&table_info) {
  auto iter = table_names_.find(table_name);
  if (iter == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  table_info = tables_[iter->second];
  return DB_SUCCESS;
}
```



#### GetTables

获取所有表的信息。遍历表名映射，根据表的 ID 获取表信息，并存储到输出参数中。返回成功或失败。



```cpp
dberr_t CatalogManager::GetTables(vector<TableInfo *> &tables) const {
  tables.clear();
  tables.reserve(tables_.size());
  for (const auto iter : tables_) {
    tables.push_back(iter.second);
  }
  return DB_SUCCESS;
}
```



#### CreateIndex

为指定表创建索引。检查表是否存在，检查索引名称是否已被占用。获取表的 ID 和信息，初始化键映射。
创建索引元数据、序列化到新页并刷新到磁盘。初始化索引信息，并更新索引名称映射、索引信息、索引元数据页映射。返回成功或失败。



```cpp
dberr_t CatalogManager::CreateIndex(const string &table_name, const string &index_name,                                       const vector<string> &index_keys, Txn *txn, IndexInfo *&index_info, const string &index_type) {
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
  Page *meta_page = buffer_pool_manager_->NewPage(meta_page_id);
  if (meta_page == nullptr) return DB_FAILED;

  // 创建索引的元数据
  IndexMetadata *index_meta = IndexMetadata::Create(index_id, index_name, table_info->GetTableId(), key_map);
  // 序列化索引的元数据到页面
  index_meta->SerializeTo(meta_page->GetData());
  // 创建索引信息对象
  index_info = IndexInfo::Create();
  index_info->Init(index_meta, table_info, buffer_pool_manager_);
  indexes_[index_id] = index_info;
  index_names_[table_name][index_name] = index_id;
  // 更新catalog元数据
  catalog_meta_->index_meta_pages_[index_id] = meta_page_id;
  buffer_pool_manager_->UnpinPage(meta_page_id, true);
  FlushCatalogMetaPage();
  return DB_SUCCESS;
}
```



#### GetIndex

获取指定表的指定索引的信息。检查表是否存在，检查索引是否存在。根据表名和索引名获取索引 ID。使用索引 ID 查找索引信息，并存储到输出参数中。返回成功或失败。

```cpp
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
```



#### GetTableIndexes

获取指定表的所有索引信息。检查表是否存在。根据表名获取该表的所有索引 ID。遍历索引 ID，根据索引 ID 获取索引信息，并存储到输出参数中。返回成功或失败。

```cpp
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
```





#### DropTable

删除指定表及其关联的所有索引。检查表是否存在。获取表的 ID 和信息。获取表中的所有索引，逐个删除。删除表的元数据页。更新表名映射、索引名称映射，刷新目录元数据页。返回成功或失败。

```cpp
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
            DropIndex(table_name, pair.first);  // 删除每个索引
        }
        //index_names_.erase(index_iter);
    }
    TableHeap *table_heap = table_info->GetTableHeap();
    if (table_heap != nullptr) {
        table_info->GetTableHeap()->DeleteTable(table_heap->GetFirstPageId());
    }
    delete table_info;
    tables_.erase(table_id);
    table_names_.erase(table_name);
    // 删除表的元数据页
    buffer_pool_manager_->DeletePage(catalog_meta_->table_meta_pages_[table_id]);
    catalog_meta_->table_meta_pages_.erase(table_id);
    // 删除表对象
    // 更新catalog元数据
    FlushCatalogMetaPage();
    return DB_SUCCESS;
}
```





#### DropIndex

删除指定表的指定索引。检查表和索引是否存在。获取索引 ID 和信息。销毁索引，删除索引的元数据页。更新索引名称映射，刷新目录元数据页。返回成功或失败。



```cpp
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
    IndexInfo *index_info = indexes_.find(index_id)->second;
    delete index_info;
    indexes_.erase(index_id);
    table_iter->second.erase(index_name);
    // 删除索引元数据页
    if (!catalog_meta_->DeleteIndexMetaPage(buffer_pool_manager_, index_id)){
        return DB_FAILED;
    }
    // 更新catalog元数据
    FlushCatalogMetaPage();
    return DB_SUCCESS;
}
```





#### FlushCatalogMetaPage

将目录元数据页中的 catalog_meta_ 对象序列化到磁盘。序列化 catalog_meta_ 到缓冲池管理器获取的页数据中。
取消页的固定，并将其标记为脏页。将页刷新到磁盘。返回成功或失败。

```cpp
dberr_t CatalogManager::FlushCatalogMetaPage() const {
  Page* meta_page=buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
  if(meta_page==nullptr)return DB_FAILED;
  char *buf=meta_page->GetData();
  catalog_meta_->SerializeTo(buf);
  buffer_pool_manager_->FlushPage(CATALOG_META_PAGE_ID);
  buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID,false);
  return DB_SUCCESS;
}
```





#### LoadTable

从元数据页加载表信息。创建 TableInfo 对象。反序列化表元数据，并创建 TableHeap 对象。初始化 TableInfo 对象，并更新表名映射、表信息。返回成功或失败。

```cpp
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
  TableHeap *table_heap = TableHeap::Create(buffer_pool_manager_, table_meta->GetFirstPageId(), table_meta->GetSchema(),log_manager_, lock_manager_);
  // 创建表信息对象
  TableInfo *table_info = TableInfo::Create();
  // 初始化表信息，关联元数据和表堆
  table_info->Init(table_meta, table_heap);
  tables_[table_id] = table_info;
  table_names_[table_meta->GetTableName()] = table_id;
  buffer_pool_manager_->UnpinPage(page_id, false);
  return DB_SUCCESS;
}
```





#### LoadIndex

从元数据页加载索引信息。参数：index_id 索引 ID，page_id 索引元数据页 ID。创建 IndexInfo 对象。反序列化索引元数据，并初始化 IndexInfo 对象。更新索引名称映射、索引信息。返回成功或失败。

```cpp
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
```





#### GetTable

根据表 ID 获取表信息。根据表 ID 获取表信息，并存储到输出参数中。返回成功或失败。

``` cpp
dberr_t CatalogManager::GetTable(const table_id_t table_id, TableInfo *&table_info) {
    auto iter = tables_.find(table_id);
    if (iter == tables_.end()) {
        return DB_TABLE_NOT_EXIST;
    }
    table_info = iter->second;
    return DB_SUCCESS;
}
```

### CatalogManager目录管理器

**CatalogManager** 类负责管理数据库的目录信息，包括表和索引的元数据。它提供了创建、删除、查询表和索引的功能，并将这些信息持久化到磁盘。而**CatalogManager**的构造函数负责初始化目录管理器，根据是否为首次初始化来决定加载策略：

- **首次初始化**：创建新的目录元数据实例
- **非首次初始化**：从磁盘加载已有的目录元数据，包括表和索引信息

构造函数的主要步骤：

1. 根据init参数决定是创建新实例还是从磁盘恢复
2. 如果是恢复模式，依次加载所有表和索引的元数据
3. 设置下一个可用的表ID和索引ID
4. 将目录元数据刷新到磁盘


```cpp
CatalogManager::CatalogManager(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager,
                               LogManager *log_manager, bool init)
    : buffer_pool_manager_(buffer_pool_manager), lock_manager_(lock_manager), log_manager_(log_manager) {
  // 第一次生成数据库
  if (init) catalog_meta_ = CatalogMeta::NewInstance();
  else {
    // 读取 meta_page 的信息
    catalog_meta_ = CatalogMeta::DeserializeFrom(buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID)->GetData());
    buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID, false);
    // 获取 table_meta
    for (auto iter: catalog_meta_->table_meta_pages_) {
      LoadTable(iter.first, iter.second);
    }
    // 获取 index_meta
    for (auto iter: catalog_meta_->index_meta_pages_) {
      LoadIndex(iter.first, iter.second);
    }
  }
  next_index_id_ = catalog_meta_->GetNextIndexId();
  next_table_id_ = catalog_meta_->GetNextTableId();
  FlushCatalogMetaPage();
}
```

## EXECUTOR ENGINE

**Executor Engine** 负责执行SQL语句的具体操作，是整个数据库系统的执行引擎。它将Planner生成的执行计划转换为具体的执行操作，协调各个模块完成数据库操作。

Executor Engine主要包括两个核心组件：

- **Planner（查询计划器）**：将Parser生成的语法树转换为可执行的查询计划
- **Executor（执行器）**：遍历查询计划树，执行具体的数据库操作

### 查询规划器（Planner）

**Planner类设计**

Planner类负责将语法树转换为执行计划，主要功能包括：

- 根据SQL语句类型选择相应的规划策略
- 为SELECT语句选择合适的扫描方式（顺序扫描或索引扫描）
- 生成各种操作的执行计划节点

```cpp
void Planner::PlanQuery(pSyntaxNode ast) {
  switch (ast->type_) {
    case kNodeSelect: {
      auto statement = make_shared<SelectStatement>(ast, context_);
      statement->SyntaxTree2Statement(ast->child_);
      plan_ = PlanSelect(statement);
      return;
    }
    case kNodeInsert: {
      auto statement = make_shared<InsertStatement>(ast, context_);
      statement->SyntaxTree2Statement(ast->child_);
      plan_ = PlanInsert(statement);
      return;
    }
    // ...existing code...
  }
}
```

### 执行引擎（Execute Engine）

#### ExecuteEngine类设计

`ExecuteEngine`是整个执行系统的核心，负责：

- 管理数据库实例
- 路由不同类型的SQL语句到相应的执行方法
- 协调Planner和Executor完成查询执行

主要成员变量：

- `dbs_`：存储所有打开的数据库实例
- `current_db_`：当前选中的数据库名称

#### 数据库管理操作

**创建数据库**

```cpp
dberr_t ExecuteEngine::ExecuteCreateDatabase(pSyntaxNode ast, ExecuteContext *context) {
  string db_name = ast->child_->val_;
  if (dbs_.find(db_name) != dbs_.end()) {
    return DB_ALREADY_EXIST;
  }
  dbs_.insert(make_pair(db_name, new DBStorageEngine(db_name, true)));
  return DB_SUCCESS;
}
```

**删除数据库**

```cpp
dberr_t ExecuteEngine::ExecuteDropDatabase(pSyntaxNode ast, ExecuteContext *context) {
  string db_name = ast->child_->val_;
  if (dbs_.find(db_name) == dbs_.end()) {
    return DB_NOT_EXIST;
  }
  remove(("./databases/" + db_name).c_str());
  delete dbs_[db_name];
  dbs_.erase(db_name);
  if (db_name == current_db_)
    current_db_ = "";
  return DB_SUCCESS;
}
```

#### 表管理操作

##### **创建表**

`ExecuteCreateTable`方法实现了完整的CREATE TABLE语句执行，包括：

- 语法树解析和验证

- 列定义解析（类型、约束等）

- 主键和唯一约束处理

- 创建具体的table表

- 自动创建相应index

  **AST语法树：**

  ![create table](D:\BaiduSyncdisk\浙江大学\软工\数据库\MiniSQL\create table.png)

```cpp
struct ParsedColumnInfo {
  std::string column_name;
  TypeId type_id{TypeId::kTypeInvalid};
  uint32_t len_for_char{0};
  bool is_not_null{false};
  bool is_unique{false};
};
dberr_t ExecuteEngine::ExecuteCreateTable(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateTable" << std::endl;
#endif

  if (context == nullptr || current_db_.empty()) {
    std::cout << "context null Or No database selected." << std::endl;
    return DB_FAILED;
  }
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }
  Txn *txn = context->GetTransaction();

  //获取表
  if (ast == nullptr || ast->type_ != kNodeCreateTable || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for CREATE TABLE statement (missing table name).";
    return DB_FAILED;
  }
  std::string table_name(ast->child_->val_);
  if (table_name.empty()) {
    LOG(ERROR) << "Syntax error: Table name for CREATE TABLE cannot be empty.";
    return DB_FAILED;
  }

  // 初始化变量解析
  std::vector<ParsedColumnInfo> parsed_col_definitions;
  std::vector<std::string> parsed_column_list_from_ast;
  std::set<std::string> parsed_column_set_for_lookup;

  pSyntaxNode col_def_list_node = ast->child_->next_;
  if (col_def_list_node == nullptr || col_def_list_node->type_ != kNodeColumnDefinitionList) {
    LOG(ERROR) << "Syntax error: Invalid column definition list in CREATE TABLE statement.";
    return DB_FAILED;
  }

  pSyntaxNode current_item_node = col_def_list_node->child_;
  pSyntaxNode col_name_node = current_item_node->child_;

  while (current_item_node != nullptr) {
    ParsedColumnInfo parsed_col_info;
    if (current_item_node->type_ == kNodeColumnDefinition) {
      if (col_name_node == nullptr || col_name_node->type_ != kNodeIdentifier || col_name_node->val_ == nullptr) {
        LOG(ERROR) << "Syntax error: Column name missing in column definition.";
        return DB_FAILED;
      }
    
    parsed_col_info.column_name = col_name_node->val_;

    pSyntaxNode col_type_node = col_name_node->next_;
    if (col_type_node == nullptr || col_type_node->type_ != kNodeColumnType) {
      LOG(ERROR) << "Syntax error: Column type missing in column definition for column '" << parsed_col_info.column_name << "'.";
      return DB_FAILED;
    }

    std::string col_type_str(col_type_node->val_);
    std::transform(col_type_str.begin(), col_type_str.end(), col_type_str.begin(), ::tolower);
    if (col_type_str == "int") {
      parsed_col_info.type_id = TypeId::kTypeInt;
      
    } else if (col_type_str == "float") {
      parsed_col_info.type_id = TypeId::kTypeFloat;
      
    } else if (col_type_str == "char") {
      parsed_col_info.type_id = TypeId::kTypeChar;
      pSyntaxNode col_length_node = col_type_node->next_;
      if (col_length_node == nullptr || col_length_node->type_ != kNodeNumber) {
        LOG(ERROR) << "Syntax error: CHAR Length missing for CHAR type in column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      std::string char_len_str(col_length_node->val_);
      if(char_len_str.empty()) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be empty for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      if (char_len_str.find('.')!=std::string::npos) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be a float for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      if (char_len_str.find('-')!=std::string::npos) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be negative for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      try {
        unsigned long char_len = std::stoul(char_len_str);
        if (char_len > std::numeric_limits<uint32_t>::max()) {
          LOG(ERROR) << "Syntax error: CHAR Length exceeds maximum allowed length for column '" << parsed_col_info.column_name << "'.";
          return DB_FAILED;
        }
        parsed_col_info.len_for_char = static_cast<uint32_t>(char_len);
      } catch (const std::invalid_argument &ia) {
          LOG(ERROR) << "Syntax error: Invalid character in length specification for CHAR column '" << parsed_col_info.column_name << "' ('" << char_len_str << "'). Length must be a positive integer.";
          return DB_FAILED;
        } catch (const std::out_of_range &oor) {
          LOG(ERROR) << "Syntax error: Length for CHAR column '" << parsed_col_info.column_name << "' is out of range for unsigned long ('" << char_len_str << "').";
          return DB_FAILED;
        }
    } else {
      LOG(ERROR) << "Syntax error: Unsupported column type '" << col_type_str << "' in column definition.";
      return DB_FAILED;
    }

    if (current_item_node->val_ != nullptr) {
      std::string constraints(current_item_node->val_);
      std::transform(constraints.begin(), constraints.end(), constraints.begin(), ::tolower);
      if (constraints.find("not null") != std::string::npos) {
        parsed_col_info.is_not_null = true;
      }
      if (constraints.find("unique") != std::string::npos) {
        parsed_col_info.is_unique = true;
      }
    }
    parsed_col_definitions.push_back(parsed_col_info);

    }else if (current_item_node->type_ == kNodeColumnList) {
      // 处理列列表
      pSyntaxNode col_list_node = current_item_node->child_;
      while (col_list_node != nullptr) {
        if (col_list_node->type_ == kNodeIdentifier && col_list_node->val_ != nullptr) {
          ParsedColumnInfo parsed_col_info;
          parsed_col_info.column_name = col_list_node->val_;
          parsed_column_list_from_ast.push_back(parsed_col_info.column_name);
          parsed_column_set_for_lookup.insert(parsed_col_info.column_name);
          col_list_node = col_list_node->next_;
        } else {
          LOG(ERROR) << "Syntax error: Expected column name in PRIMARY KEY constraint for table '" << table_name << "'.";
          return DB_FAILED;
        }
        col_list_node = col_list_node->next_;
      }
    } else {
      LOG(ERROR) << "Syntax error: Invalid node type in column definition list.";
      return DB_FAILED;
    }  
    current_item_node = current_item_node->next_;
  }

  if (parsed_col_definitions.empty() && parsed_column_list_from_ast.empty()) {
    LOG(ERROR) << "Syntax error: No columns defined in CREATE TABLE statement for table '" << table_name << "'.";
    return DB_FAILED;
  }

  // 验证主键的列名是否都在已经定义的列中
  for (const auto &col_name : parsed_column_list_from_ast) {
    bool check = false;
    for (const auto &parsed_col_def : parsed_col_definitions) {
      if (parsed_col_def.column_name == col_name) {
        // 如果列名在定义中，跳过检查
        check = true;
        break;
      }
    }
    if (!check) {
      LOG(ERROR) << "Syntax error: Column '" << col_name << "' in PRIMARY KEY constraint not defined in table '" << table_name << "'.";
      ExecuteInformation(DB_COLUMN_NAME_NOT_EXIST); 
      return DB_COLUMN_NAME_NOT_EXIST;
    }
  }

  // 开始创建Column对象
  std::vector<Column*> columns;
  uint32_t index = 0;
  for (const auto &parsed_col_def : parsed_col_definitions) {
    bool is_primary_key = (parsed_column_set_for_lookup.find(parsed_col_def.column_name) != parsed_column_set_for_lookup.end());
    bool is_unique = parsed_col_def.is_unique || is_primary_key;
    bool is_nullable = !parsed_col_def.is_not_null && !is_primary_key; // 主键列不能为NULL
    
    Column *column = nullptr;
    if (parsed_col_def.type_id == TypeId::kTypeChar) {
      column = new Column(parsed_col_def.column_name, parsed_col_def.type_id, parsed_col_def.len_for_char, index, is_nullable, is_unique);
    } else {
      column = new Column(parsed_col_def.column_name, parsed_col_def.type_id, index, is_nullable, is_unique);
    }
    columns.push_back(column);
    index++;
  }

  // 根据columns创建Schema
  TableSchema *schema = new TableSchema(columns, true);

  // 调用CatalogManager的CreateTable方法
  TableInfo *table_info = nullptr;
  dberr_t create_result = catalog_manager->CreateTable(table_name, schema, txn, table_info);

  delete schema; // 释放Schema内存(如果CreateTable成功，里面是深拷贝会开新的Schema对象)

  if (create_result != DB_SUCCESS) {
    LOG(ERROR) << "Failed to create table '" << table_name << "' in database '" << current_db_ << "'. Error code: " << create_result;
    ExecuteInformation(create_result);
    return create_result;
  }

  // 创建primary_key的索引（如果有primary_key）
  if(!parsed_column_list_from_ast.empty()) {
    std::string index_name = table_name + "_primary_key";
    IndexInfo *index_info = nullptr;
    dberr_t index_create_result = catalog_manager->CreateIndex(table_name, index_name, parsed_column_list_from_ast, txn, index_info,"bptree");
    if (index_create_result != DB_SUCCESS) {
      LOG(ERROR) << "Failed to create primary key index '" << index_name << "' for table '" << table_name << "'. Error code: " << index_create_result<<endl;
      // 尝试回滚，删除刚才创建的table
      dberr_t drop_table_result = catalog_manager->DropTable(table_name);
      if (drop_table_result != DB_SUCCESS) {
        LOG(ERROR) << "Failed to rollback table '" << table_name << "' after index creation failure."<<endl;
      }
      else {
        LOG(INFO) << "Have Rolled back table '" << table_name << "' after Primary_Key index creation failure."<<endl;
      }
      ExecuteInformation(index_create_result);
      return index_create_result;
    }
  }

  // 根据其他unique键，创建索引
  for (const auto& parsed_col_def : parsed_col_definitions) {
    if (parsed_col_def.is_unique && parsed_column_set_for_lookup.find(parsed_col_def.column_name) == parsed_column_set_for_lookup.end()) {
      std::string index_name = table_name + "_" + parsed_col_def.column_name + "_unique";
      IndexInfo *index_info = nullptr;
      dberr_t index_create_result = catalog_manager->CreateIndex(table_name, index_name, {parsed_col_def.column_name}, txn, index_info,"bptree");
      if (index_create_result != DB_SUCCESS) {
        LOG(WARNING) << "Failed to create unique index '" << index_name << "' for column '" << parsed_col_def.column_name << "' in table '" << table_name <<"'"<<endl;
      }
    }
  }

  LOG(INFO) << "Table '" << table_name << "' created successfully."<< std::endl;

  return DB_SUCCESS;
}
```

##### **删除表**

`ExecuteDropTable`方法实现了DROP TABLE语句的执行：

**主要实现步骤：**

1. **上下文验证**：检查执行上下文和数据库选择状态
2. **AST结构验证**：验证语法树的完整性和节点类型
3. **表名提取**：从AST中提取要删除的表名
4. **表删除**：调用CatalogManager删除表及其关联索引
5. **结果反馈**：输出删除结果信息

**AST语法树结构：**

kNodeDropTable
└── kNodeIdentifier (table_name)

```cpp
dberr_t ExecuteEngine::ExecuteDropTable(pSyntaxNode ast, ExecuteContext *context) {
  // 步骤1: 验证执行上下文和数据库状态
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  // 步骤2: 获取CatalogManager
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }

  // 步骤3: 验证AST结构并提取表名
  if (ast == nullptr || ast->type_ != kNodeDropTable || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for DROP TABLE statement.";
    return DB_FAILED;
  }
  std::string table_name(ast->child_->val_);
  if (table_name.empty()) {
    LOG(ERROR) << "Syntax error: Table name for DROP TABLE cannot be empty.";
    return DB_FAILED;
  }

  // 步骤4: 调用CatalogManager删除表
  dberr_t res = catalog_manager->DropTable(table_name);
  if (res != DB_SUCCESS) {
    ExecuteInformation(res);
    return res;
  }

  // 步骤5: 输出成功信息
  std::cout << "Table [" << table_name << "] dropped successfully." << std::endl;
  return DB_SUCCESS;
}
```

#### 索引管理操作

##### **显示索引**

`ExecuteShowIndexes`方法实现了SHOW INDEXES语句的执行：

**主要实现步骤：**

1. **上下文验证**：检查执行上下文和数据库选择状态
2. **表信息获取**：获取数据库中的所有表
3. **索引收集**：遍历每个表，收集所有索引信息
4. **格式化输出**：计算列宽并以表格形式显示
5. **结果汇总**：输出最终的索引列表

**AST语法树结构：**

kNodeShowIndexes
(无子节点)

```cpp
dberr_t ExecuteEngine::ExecuteShowIndexes(pSyntaxNode ast, ExecuteContext *context) {
  // 步骤1: 验证执行上下文和数据库选择状态
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }

  // 步骤2: 获取数据库中的所有表
  std::vector<TableInfo *> all_tables;
  dberr_t get_tables_result = catalog_manager->GetTables(all_tables);
  if (get_tables_result != DB_SUCCESS) {
    if (get_tables_result == DB_TABLE_NOT_EXIST) {
      std::cout << "No index exists in database '" << current_db_ << "' (no tables found)." << std::endl;
      return DB_SUCCESS;
    }
    ExecuteInformation(get_tables_result);
    return get_tables_result;
  }

  // 步骤3: 初始化输出控制变量
  bool found_any_index = false;
  std::stringstream output_stream;
  ResultWriter result_writer(output_stream);
  bool is_first_table_output = true;

  // 步骤4: 遍历每个表收集索引信息
  for (TableInfo *table_info_ptr : all_tables) {
    if (table_info_ptr == nullptr) {
      continue;
    }

    std::string table_name = table_info_ptr->GetTableName();
    std::vector<IndexInfo *> table_indexes;

    // 获取当前表的所有索引
    dberr_t get_indexes_result = catalog_manager->GetTableIndexes(table_name, table_indexes);
    if (get_indexes_result != DB_SUCCESS && get_indexes_result != DB_INDEX_NOT_FOUND) {
      ExecuteInformation(get_indexes_result);
      return get_indexes_result;
    }

    // 如果当前表没有索引，跳过
    if (table_indexes.empty()) {
      continue;
    }

    found_any_index = true;

    // 步骤5: 格式化输出当前表的索引列表
    if (!is_first_table_output) {
      output_stream << std::endl;  // 表之间添加空行
    }
    is_first_table_output = false;

    // 计算列宽
    std::string table_header = "Indexes_in_" + table_name;
    int column_width = static_cast<int>(table_header.length());
    for (IndexInfo *index_info_ptr : table_indexes) {
      if (index_info_ptr != nullptr) {
        column_width = std::max(column_width, static_cast<int>(index_info_ptr->GetIndexName().length()));
      }
    }
    
    std::vector<int> column_widths = {column_width};

    // 输出表头
    result_writer.Divider(column_widths);
    result_writer.BeginRow();
    result_writer.WriteHeaderCell(table_header, column_width);
    result_writer.EndRow();
    result_writer.Divider(column_widths);

    // 输出索引名称
    for (IndexInfo *index_info_ptr : table_indexes) {
      if (index_info_ptr != nullptr) {
        result_writer.BeginRow();
        result_writer.WriteCell(index_info_ptr->GetIndexName(), column_width);
        result_writer.EndRow();
      }
    }
    result_writer.Divider(column_widths);
  }

  // 步骤6: 输出最终结果
  if (found_any_index) {
    std::cout << output_stream.str();
  } else {
    std::cout << "No index exists in database '" << current_db_ << "'." << std::endl;
  }
  
  return DB_SUCCESS;
}
```

##### **创建索引**

`ExecuteCreateIndex`方法实现了CREATE INDEX语句的执行：

**主要实现步骤：**

1. **上下文验证**：检查执行上下文和数据库状态
2. **AST解析**：解析索引名、表名和列名列表
3. **索引类型解析**：解析可选的索引类型参数
4. **存在性验证**：验证表和列的存在性
5. **索引创建**：在CatalogManager中创建索引
6. **数据填充**：为现有数据构建索引

**AST语法树结构：**

kNodeCreateIndex
├── kNodeIdentifier (index_name)
├── kNodeIdentifier (table_name)  
├── kNodeColumnList
│   └── kNodeIdentifier (column_names...)
└── kNodeIndexType (optional)
    └── kNodeIdentifier (index_type)

```cpp
dberr_t ExecuteEngine::ExecuteCreateIndex(pSyntaxNode ast, ExecuteContext *context) {
  // 步骤1: 验证执行上下文
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();
  Txn *txn = context->GetTransaction();

  // 步骤2: 解析AST获取索引名称
  if (ast == nullptr || ast->type_ != kNodeCreateIndex || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX.";
    return DB_FAILED;
  }
  std::string index_name(ast->child_->val_);

  // 步骤3: 解析表名
  pSyntaxNode table_name_node = ast->child_->next_;
  if (table_name_node == nullptr || table_name_node->type_ != kNodeIdentifier || table_name_node->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX (missing table name).";
    return DB_FAILED;
  }
  std::string table_name(table_name_node->val_);

  // 步骤4: 解析列名列表
  pSyntaxNode column_list_node = table_name_node->next_;
  if (column_list_node == nullptr || column_list_node->type_ != kNodeColumnList || column_list_node->child_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX (missing column list).";
    return DB_FAILED;
  }
  
  std::vector<std::string> index_column_names;
  pSyntaxNode current_column_node = column_list_node->child_;
  while (current_column_node != nullptr) {
    if (current_column_node->type_ != kNodeIdentifier || current_column_node->val_ == nullptr) {
      LOG(ERROR) << "Syntax error: Expected column name in index key list.";
      return DB_FAILED;
    }
    index_column_names.push_back(std::string(current_column_node->val_));
    current_column_node = current_column_node->next_;
  }

  // 步骤5: 解析索引类型（可选）
  std::string index_type = "bptree";  // 默认类型
  pSyntaxNode index_type_node = column_list_node->next_;
  if (index_type_node != nullptr && index_type_node->type_ == kNodeIndexType) {
    if (index_type_node->child_ != nullptr && index_type_node->child_->type_ == kNodeIdentifier &&
        index_type_node->child_->val_ != nullptr) {
      index_type = index_type_node->child_->val_;
    }
  }

  // 步骤6: 验证表的存在性
  TableInfo *table_info = nullptr;
  dberr_t get_table_result = catalog_manager->GetTable(table_name, table_info);
  if (get_table_result != DB_SUCCESS) {
    ExecuteInformation(get_table_result);
    return get_table_result;
  }

  // 步骤7: 验证列的存在性并构建列索引映射
  std::vector<uint32_t> column_index_mapping;
  TableSchema *table_schema = table_info->GetSchema();
  for (const std::string &column_name : index_column_names) {
    uint32_t column_index;
    if (table_schema->GetColumnIndex(column_name, column_index) != DB_SUCCESS) {
      LOG(ERROR) << "Column '" << column_name << "' not found in table '" << table_name << "'.";
      ExecuteInformation(DB_COLUMN_NAME_NOT_EXIST);
      return DB_COLUMN_NAME_NOT_EXIST;
    }
    column_index_mapping.push_back(column_index);
  }

  // 步骤8: 在CatalogManager中创建索引
  IndexInfo *created_index_info = nullptr;
  dberr_t create_index_result = catalog_manager->CreateIndex(
      table_name, index_name, index_column_names, txn, created_index_info, index_type);
  if (create_index_result != DB_SUCCESS) {
    ExecuteInformation(create_index_result);
    return create_index_result;
  }

  // 步骤9: 为现有数据构建索引
  TableHeap *table_heap = table_info->GetTableHeap();
  Index *index_structure = created_index_info->GetIndex();
  
  for (TableIterator table_iter = table_heap->Begin(txn); table_iter != table_heap->End(); ++table_iter) {
    Row current_row(table_iter->GetRowId());
    if (!table_heap->GetTuple(&current_row, txn)) {
      continue; 
    }
    
    RowId row_id = current_row.GetRowId();
    
    // 构建索引键行
    std::vector<Field> index_key_fields;
    for (uint32_t column_index : column_index_mapping) {
      index_key_fields.push_back(*(current_row.GetField(column_index))); 
    }
    Row index_key_row(index_key_fields);

    // 将记录插入索引
    if (index_structure->InsertEntry(index_key_row, row_id, txn) != DB_SUCCESS) {
      LOG(ERROR) << "Failed to insert entry into index during initial population.";
      catalog_manager->DropIndex(table_name, index_name);
      return DB_FAILED;
    }
  }
  
  std::cout << "Index [" << index_name << "] created successfully on table [" << table_name << "]." << std::endl;
  return DB_SUCCESS;
}
```

##### **删除索引**

`ExecuteDropIndex`方法实现了DROP INDEX语句的执行：

**主要实现步骤：**

1. **上下文验证**：检查执行上下文和数据库状态
2. **AST解析**：验证语法树结构并提取索引名
3. **表遍历**：获取数据库中的所有表
4. **索引查找**：在每个表中查找目标索引
5. **索引删除**：找到后调用CatalogManager删除索引

**AST语法树结构：**

kNodeDropIndex
└── kNodeIdentifier (index_name)

```cpp
dberr_t ExecuteEngine::ExecuteDropIndex(pSyntaxNode ast, ExecuteContext *context) {
  // 步骤1: 验证执行上下文
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();

  // 步骤2: 解析AST获取索引名称
  if (ast == nullptr || ast->type_ != kNodeDropIndex || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for DROP INDEX statement.";
    return DB_FAILED;
  }
  std::string target_index_name(ast->child_->val_);
  if (target_index_name.empty()) {
    LOG(ERROR) << "Syntax error: Index name for DROP INDEX cannot be empty.";
    return DB_FAILED;
  }

  // 步骤3: 获取数据库中的所有表
  std::vector<TableInfo *> database_tables;
  dberr_t get_tables_result = catalog_manager->GetTables(database_tables);
  if (get_tables_result != DB_SUCCESS) {
    if (get_tables_result == DB_TABLE_NOT_EXIST) {
      ExecuteInformation(DB_INDEX_NOT_FOUND);
      return DB_INDEX_NOT_FOUND;
    }
    ExecuteInformation(get_tables_result);
    return get_tables_result;
  }

  // 步骤4: 遍历所有表查找目标索引
  for (TableInfo *table_info_ptr : database_tables) {
    if (table_info_ptr == nullptr) {
      continue;
    }

    std::string current_table_name = table_info_ptr->GetTableName();
    std::vector<IndexInfo *> table_index_list;

    // 获取当前表的所有索引
    dberr_t get_table_indexes_result = catalog_manager->GetTableIndexes(current_table_name, table_index_list);
    
    if (get_table_indexes_result == DB_SUCCESS) {
      // 在当前表的索引中查找目标索引
      for (IndexInfo *index_info_ptr : table_index_list) {
        if (index_info_ptr == nullptr) {
          continue;
        }

        if (index_info_ptr->GetIndexName() == target_index_name) {
          // 步骤5: 找到目标索引，执行删除操作
          dberr_t drop_index_result = catalog_manager->DropIndex(current_table_name, target_index_name);
          if (drop_index_result != DB_SUCCESS) {
            ExecuteInformation(drop_index_result);
            return drop_index_result;
          }
          std::cout << "Index [" << target_index_name << "] dropped successfully from table [" << current_table_name << "]." << std::endl;
          return DB_SUCCESS;
        }
      }
    } else if (get_table_indexes_result != DB_INDEX_NOT_FOUND) {
      LOG(ERROR) << "Error fetching indexes for table " << current_table_name << " during DROP INDEX operation.";
      ExecuteInformation(get_table_indexes_result);
      return get_table_indexes_result;
    }
  }
  
  // 未找到目标索引
  ExecuteInformation(DB_INDEX_NOT_FOUND);
  return DB_INDEX_NOT_FOUND;
}
```

#### 文件执行操作

##### **执行SQL脚本文件**

`ExecuteExecfile`方法实现了EXECFILE语句的执行：

**主要实现步骤：**

1. **AST解析**：验证语法树结构并提取文件名
2. **文件操作**：打开并验证SQL脚本文件
3. **语句分割**：按分号分割文件中的SQL语句
4. **语句解析**：逐个解析SQL语句生成AST
5. **语句执行**：递归调用Execute方法执行每个语句
6. **错误处理**：处理语法错误和执行错误

**AST语法树结构：**

kNodeExecFile
└── kNodeString/kNodeIdentifier (filename)

```cpp
dberr_t ExecuteEngine::ExecuteExecfile(pSyntaxNode ast, ExecuteContext *context) {
  // 步骤1: 解析AST获取文件名
  if (ast == nullptr || ast->type_ != kNodeExecFile || ast->child_ == nullptr ||
      (ast->child_->type_ != kNodeString && ast->child_->type_ != kNodeIdentifier) ||
      ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for EXECFILE statement.";
    return DB_FAILED;
  }
  std::string script_filename(ast->child_->val_);
  if (script_filename.empty()) {
    LOG(ERROR) << "Syntax error: Filename for EXECFILE cannot be empty.";
    return DB_FAILED;
  }

  // 步骤2: 打开并验证SQL脚本文件
  std::ifstream script_file(script_filename);
  if (!script_file.is_open()) {
    LOG(ERROR) << "Cannot open file '" << script_filename << "' for EXECFILE.";
    std::cout << "Error: Cannot open SQL script file '" << script_filename << "'." << std::endl;
    return DB_FAILED;
  }

  std::cout << "Executing SQL script file [" << script_filename << "] ..." << std::endl;
  
  // 步骤3: 初始化执行状态变量
  std::string statement_buffer;
  char current_char;
  dberr_t execution_status = DB_SUCCESS;
  int current_line_number = 0;

  // 步骤4: 逐字符读取和处理文件内容
  while (script_file.get(current_char)) {
    statement_buffer += current_char;
    if (current_char == '\n') {
      current_line_number++;
    }

    if (current_char == ';') {  // 遇到语句结束符
      // 清理语句字符串
      statement_buffer.erase(0, statement_buffer.find_first_not_of(" \t\n\r\f\v"));
      statement_buffer.erase(statement_buffer.find_last_not_of(" \t\n\r\f\v") + 1);

      if (statement_buffer.empty() || statement_buffer == ";") {
        statement_buffer.clear();
        continue;
      }

      // 步骤5: 解析SQL语句
      MinisqlParserInit();
      YY_BUFFER_STATE lexer_buffer = yy_scan_string(statement_buffer.c_str());
      if (lexer_buffer == nullptr) {
        LOG(ERROR) << "Failed to create Flex buffer for SQL statement.";
        execution_status = DB_FAILED;
        MinisqlParserFinish();
        break;
      }
      
      int parsing_result = yyparse();
      yy_delete_buffer(lexer_buffer);
      pSyntaxNode statement_ast = MinisqlGetParserRootNode();

      if (parsing_result != 0 || statement_ast == nullptr || MinisqlParserGetError() != 0) {
        LOG(ERROR) << "Syntax error in file '" << script_filename << "' around line " << current_line_number;
        if (MinisqlParserGetErrorMessage() != nullptr) {
          std::cout << "Error (approx. line " << current_line_number << "): " << MinisqlParserGetErrorMessage() << std::endl;
        }
        execution_status = DB_FAILED;
        DestroySyntaxTree();
        MinisqlParserFinish();
        break;
      }

      // 步骤6: 执行解析得到的SQL语句
      dberr_t statement_execution_result = Execute(statement_ast);
      
      DestroySyntaxTree();
      MinisqlParserFinish();

      if (statement_execution_result == DB_QUIT) {
        execution_status = DB_QUIT;
        std::cout << "QUIT command encountered in script. Halting script execution." << std::endl;
        break;
      }
      
      if (statement_execution_result != DB_SUCCESS) {
        LOG(WARNING) << "Error executing statement from file around line " << current_line_number;
        execution_status = statement_execution_result;
        break;
      }
      
      statement_buffer.clear();
    }
  }

  script_file.close();

  // 步骤7: 输出执行结果摘要
  if (execution_status == DB_SUCCESS) {
    std::cout << "SQL script file [" << script_filename << "] executed successfully." << std::endl;
  } else if (execution_status != DB_QUIT) {
    std::cout << "Execution of SQL script file [" << script_filename << "] encountered errors." << std::endl;
  }
  
  return DB_SUCCESS;
}
```

##### **退出数据库**

`ExecuteQuit`方法实现了QUIT语句的执行：

**主要实现步骤：**

1. **执行信息输出**：调用ExecuteInformation显示退出信息
2. **返回退出状态**：返回DB_QUIT状态码

**AST语法树结构：**

kNodeQuit
(无子节点)

```cpp
dberr_t ExecuteEngine::ExecuteQuit(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteQuit" << std::endl;
#endif
  // 步骤1: 输出退出信息
  ExecuteInformation(DB_QUIT);
  
  // 步骤2: 返回退出状态码
  return DB_QUIT;
}
```
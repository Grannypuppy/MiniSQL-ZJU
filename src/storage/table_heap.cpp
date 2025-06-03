#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
bool TableHeap::InsertTuple(Row &row, Txn *txn) {
  if (row.GetSerializedSize(schema_) >= TablePage::SIZE_MAX_ROW) return false;
  page_id_t p = first_page_id_;
  page_id_t last_p = first_page_id_;

  bool success;
  while (p != INVALID_PAGE_ID) {
    TablePage *page = static_cast<TablePage *>(buffer_pool_manager_->FetchPage(p));
    page->WLatch();
    success = page->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);
    page->WUnlatch();

    if (success) {
      buffer_pool_manager_->UnpinPage(p, true);
      return true;
    }

    last_p = p;
    page_id_t next_ = page->GetNextPageId();
    buffer_pool_manager_->UnpinPage(p, false);
    p = next_;
  }

  // 没找到插入位置，新建一页
  page_id_t new_page_id;
  auto new_page_ = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(new_page_id));
  if (new_page_ == nullptr) return false;

  new_page_->Init(new_page_id, last_p, log_manager_, txn);
  if (last_p != INVALID_PAGE_ID) {
    auto last_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(last_p));
    last_page->WLatch();
    last_page->SetNextPageId(new_page_id);
    last_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(last_p, true);
  } else {
    first_page_id_ = new_page_id;
  }

  new_page_->WLatch();
  success = new_page_->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);
  new_page_->WUnlatch();
  buffer_pool_manager_->UnpinPage(new_page_id, true);

  return success;
}

bool TableHeap::MarkDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  // If the page could not be found, then abort the recovery.
  if (page == nullptr) {
    return false;
  }
  // Otherwise, mark the tuple as deleted.
  page->WLatch();
  page->MarkDelete(rid, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
  return true;
}

/**
 * TODO: Student Implement
 */
bool TableHeap::UpdateTuple(Row &row, const RowId &rid, Txn *txn) {
  
  if (rid.GetPageId() == INVALID_PAGE_ID) {
    LOG(WARNING) << "UpdateTuple called with invalid RowId.";
    return false;
  }
  
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if (page == nullptr) return false;

  Row old_row_(rid);
  
  page->WLatch();
  bool success = page->UpdateTuple(row, &old_row_, schema_, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(rid.GetPageId(), success);

  if (success) {
    row.SetRowId(rid);
    return true;
  } else {
    if (!MarkDelete(rid, txn)) return false;
    return InsertTuple(row, txn);
  }
}

/**
 * TODO: Student Implement
 */
void TableHeap::ApplyDelete(const RowId &rid, Txn *txn) {
  //找到tuple在的page
  Page *raw = buffer_pool_manager_->FetchPage(rid.GetPageId());
  assert(raw != nullptr);
  auto page = reinterpret_cast<TablePage *>(raw);

  //删除tuple
  page->WLatch();
  page->ApplyDelete(rid, txn, log_manager_);
  page->WUnlatch();

  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

void TableHeap::RollbackDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  assert(page != nullptr);
  // Rollback to delete.
  page->WLatch();
  page->RollbackDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

/**
 * TODO: Student Implement
 */
bool TableHeap::GetTuple(Row *row, Txn *txn) {
  const RowId rid = row->GetRowId();
  page_id_t page_id = rid.GetPageId();

  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));

  if (page == nullptr) return false;
  bool ok = page->GetTuple(row, schema_, txn, lock_manager_);
  buffer_pool_manager_->UnpinPage(page_id, false);

  return ok;
}

void TableHeap::DeleteTable(page_id_t page_id) {
  if (page_id != INVALID_PAGE_ID) {
    auto temp_table_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));  // 删除table_heap
    if (temp_table_page->GetNextPageId() != INVALID_PAGE_ID)
      DeleteTable(temp_table_page->GetNextPageId());
    buffer_pool_manager_->UnpinPage(page_id, false);
    buffer_pool_manager_->DeletePage(page_id);
  } else {
    DeleteTable(first_page_id_);
  }
}

/**
 * TODO: Student Implement
 */
TableIterator TableHeap::Begin(Txn *txn) {
  page_id_t pid = first_page_id_;

  while (pid != INVALID_PAGE_ID) {
    auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(pid));
    RowId first_rid;
    if (page->GetFirstTupleRid(&first_rid)) {
      buffer_pool_manager_->UnpinPage(pid, false);
      return TableIterator(this, first_rid, txn);
    }

    page_id_t next = page->GetNextPageId();
    buffer_pool_manager_->UnpinPage(pid, false);
    pid = next;
  }

  return End();
}

/**
 * TODO: Student Implement
 */
TableIterator TableHeap::End() { return TableIterator(this, INVALID_ROWID, nullptr); }

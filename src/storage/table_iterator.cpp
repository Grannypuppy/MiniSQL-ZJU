#include "storage/table_iterator.h"

#include "common/macros.h"
#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
TableIterator::TableIterator(TableHeap *table_heap, RowId rid, Txn *txn) : table_heap_(table_heap), current_rid_(rid), txn_(txn) {
  // 判断我的rid是否有效
  if (current_rid_ == INVALID_ROWID || table_heap_ == nullptr) return;
  current_row_ = Row(current_rid_);
  if(!table_heap_->GetTuple(&current_row_, txn_)) 
  {
    current_rid_.Set(INVALID_PAGE_ID, 0);
  }
}

TableIterator::TableIterator(const TableIterator &other) {
  // 浅拷贝
  table_heap_ = other.table_heap_;
  txn_ = other.txn_;
  current_row_ = other.current_row_;
  current_rid_ = other.current_rid_;
}

TableIterator::~TableIterator() {
}

bool TableIterator::operator==(const TableIterator &itr) const {
  return (table_heap_ == itr.table_heap_) && (current_rid_ == itr.current_rid_);
}

bool TableIterator::operator!=(const TableIterator &itr) const {
  return !((table_heap_ == itr.table_heap_) && (current_rid_ == itr.current_rid_));
}

const Row &TableIterator::operator*() {
  return current_row_;
}

Row *TableIterator::operator->() {
  return &current_row_;
}

TableIterator &TableIterator::operator=(const TableIterator &itr) noexcept {
  table_heap_ = itr.table_heap_;
  txn_ = itr.txn_;
  current_row_ = itr.current_row_;
  current_rid_ = itr.current_rid_;
  return *this;
}

// ++iter
TableIterator &TableIterator::operator++() {
  if (current_rid_ == INVALID_ROWID || table_heap_ == nullptr) return *this;

  auto bpm = table_heap_->buffer_pool_manager_;
  page_id_t cur_page_id = current_rid_.GetPageId();
  auto page = reinterpret_cast<TablePage *>(bpm->FetchPage(cur_page_id));
  RowId next_rid;

  // 页内下一条
  if (page->GetNextTupleRid(current_rid_, &next_rid)) {
    bpm->UnpinPage(cur_page_id, false);
    current_rid_ = next_rid;
    current_row_ = Row(current_rid_);
    bool ok = table_heap_->GetTuple(&current_row_, txn_);
    ASSERT(ok, "Operator++ GetTuple failed"); // 必须读取成功
    return *this;
  }

  // 找下一页
  page_id_t next_page_id = page->GetNextPageId();
  bpm->UnpinPage(cur_page_id, false);
  
  while (next_page_id != INVALID_PAGE_ID) {
    auto page_next = reinterpret_cast<TablePage *>(bpm->FetchPage(next_page_id));
    
    // 无法获取下一页，设为 End()
    if (page_next == nullptr) {
      current_rid_.Set(INVALID_PAGE_ID, 0);
      bpm->UnpinPage(next_page_id, false);
      return *this;
    }

    // 从新页面获得元组
    if (page_next->GetFirstTupleRid(&next_rid)) {
      bpm->UnpinPage(next_page_id, false);
      current_rid_ = next_rid;
      current_row_ = Row(current_rid_);
      bool ok = table_heap_->GetTuple(&current_row_, txn_);
      ASSERT(ok, "Operator++ GetTuple failed in a new page");
      return *this;
    }

    bpm->UnpinPage(next_page_id, false);
    next_page_id  = page_next->GetNextPageId();
  }

  // 没找到
  current_rid_ = INVALID_ROWID;
  return *this;
}

// iter++
TableIterator TableIterator::operator++(int) {
  TableIterator tmp = *this;
  ++(*this);
  return tmp;
}

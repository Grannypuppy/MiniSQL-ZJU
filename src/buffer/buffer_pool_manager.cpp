#include "buffer/buffer_pool_manager.h"

#include "glog/logging.h"
#include "page/bitmap_page.h"

static const char EMPTY_PAGE_DATA[PAGE_SIZE] = {0};

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
        : pool_size_(pool_size), disk_manager_(disk_manager) {
    pages_ = new Page[pool_size_];
    replacer_ = new LRUReplacer(pool_size_);
    for (size_t i = 0; i < pool_size_; i++) {
        free_list_.emplace_back(i);
    }
}

BufferPoolManager::~BufferPoolManager() {
    for (auto page : page_table_) {
        FlushPage(page.first);
    }
    delete[] pages_;
    delete replacer_;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::FetchPage(page_id_t page_id) {
    if (page_table_.count(page_id) != 0) {
        frame_id_t frame_id = page_table_[page_id];
        Page &page = pages_[frame_id];
        page.pin_count_++;
        replacer_->Pin(frame_id);
        return &page;
    }

    frame_id_t frame_id = TryToFindFreePage();
    if (frame_id == -1)
        return nullptr;

    //判断是否为脏页
    Page &victim = pages_[frame_id];
    if (victim.IsDirty()) {
        disk_manager_->WritePage(victim.page_id_, victim.data_);
    }
    page_table_.erase(victim.page_id_);

    //写入新页
    victim.page_id_ = page_id;
    victim.pin_count_ = 1;
    victim.is_dirty_ = false;
    page_table_[page_id] = frame_id;
    disk_manager_->ReadPage(page_id, victim.data_);

    return &victim;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::NewPage(page_id_t &page_id) {
    frame_id_t frame_id = TryToFindFreePage();
    if (frame_id == INVALID_PAGE_ID) return nullptr;

    //如果是脏页，需要写入磁盘
    Page &page = pages_[frame_id];
    if (page.IsDirty()) {
        disk_manager_->WritePage(page.page_id_, page.data_);
    }

    page_id = AllocatePage();

    //替换
    page_table_.erase(page.page_id_);
    page_table_[page_id] = frame_id;
    page.page_id_ = page_id;
    page.pin_count_ = 1;
    page.is_dirty_ = false;
    memset(page.data_, 0, PAGE_SIZE);

    return &page;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::DeletePage(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return true;

    frame_id_t frame_id = it->second;
    Page &page = pages_[frame_id];
    if (page.pin_count_ > 0) return false;

    DeallocatePage(page_id);
    page_table_.erase(it);
    replacer_->Pin(frame_id);
    page.page_id_ = INVALID_PAGE_ID;
    page.pin_count_ = 0;
    page.is_dirty_ = false;
    memset(page.data_, 0, PAGE_SIZE);
    free_list_.emplace_back(frame_id);

    return false;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t frame_id = it->second;
    Page &page =pages_[frame_id];
    
    if (page.pin_count_ == 0) {
        LOG(WARNING) << "PAGE ALREADY BE UNPINNED" << page_id;
        return false;
    }

    if (is_dirty) page.is_dirty_ = true;
    page.pin_count_--;
    if (page.pin_count_ == 0) {
        replacer_->Unpin(frame_id);
    }

    return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::FlushPage(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t frame_id = it->second;
    Page &page = pages_[frame_id];

    disk_manager_->WritePage(page.page_id_, page.data_);
    page.is_dirty_ = false;
    return true;
}

//利用LRU尝试寻找空闲页
frame_id_t BufferPoolManager::TryToFindFreePage() {
    if (!free_list_.empty()) {
        frame_id_t frame_id = free_list_.front();
        free_list_.pop_front();
        return frame_id;
    }
    frame_id_t victim;
    if (replacer_->Victim(&victim)) return victim;
    return INVALID_FRAME_ID;
}

page_id_t BufferPoolManager::AllocatePage() {
    int next_page_id = disk_manager_->AllocatePage();
    return next_page_id;
}

void BufferPoolManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {
    disk_manager_->DeAllocatePage(page_id);
}

bool BufferPoolManager::IsPageFree(page_id_t page_id) {
    return disk_manager_->IsPageFree(page_id);
}

// Only used for debug
bool BufferPoolManager::CheckAllUnpinned() {
    bool res = true;
    for (size_t i = 0; i < pool_size_; i++) {
        if (pages_[i].pin_count_ != 0) {
            res = false;
            LOG(ERROR) << "page " << pages_[i].page_id_ << " pin count:" << pages_[i].pin_count_ << endl;
        }
    }
    return res;
}
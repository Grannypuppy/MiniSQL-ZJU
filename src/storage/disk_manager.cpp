#include "storage/disk_manager.h"

#include <sys/stat.h>

#include <filesystem>
#include <stdexcept>

#include "glog/logging.h"
#include "page/bitmap_page.h"

DiskManager::DiskManager(const std::string &db_file) : file_name_(db_file) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
  // directory or file does not exist
  if (!db_io_.is_open()) {
    db_io_.clear();
    // create a new file
    std::filesystem::path p = db_file;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    db_io_.open(db_file, std::ios::binary | std::ios::trunc | std::ios::out);
    db_io_.close();
    // reopen with original mode
    db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_io_.is_open()) {
      throw std::exception();
    }
  }
  ReadPhysicalPage(META_PAGE_ID, meta_data_);
}

void DiskManager::Close() {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  WritePhysicalPage(META_PAGE_ID, meta_data_);
  if (!closed) {
    db_io_.close();
    closed = true;
  }
}

void DiskManager::ReadPage(page_id_t logical_page_id, char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  ReadPhysicalPage(MapPageId(logical_page_id), page_data);
}

void DiskManager::WritePage(page_id_t logical_page_id, const char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  WritePhysicalPage(MapPageId(logical_page_id), page_data);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::AllocatePage() {
  DiskFileMetaPage *meta_page = reinterpret_cast<DiskFileMetaPage *>(meta_data_);
  uint32_t extents_nums = meta_page->GetExtentNums();
  uint32_t extent_id;

  // 查找第一块空闲分区
  for (extent_id = 0; extent_id < extents_nums; extent_id++) {
    if (meta_page->GetExtentUsedPage(extent_id) < DiskManager::BITMAP_SIZE) break;
  }

  // 找不到
  if (extent_id >= (PAGE_SIZE - 8) / 4) {
    return INVALID_PAGE_ID;
  }

  // 更新位图，在内存中分配数据页
  page_id_t bitmap_physical_id = 1 + extent_id * (BITMAP_SIZE + 1); // bitmap能管理的数据页+自己
  char buf[PAGE_SIZE];
  ReadPhysicalPage(bitmap_physical_id, buf);
  BitmapPage<PAGE_SIZE> *bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(buf);
  uint32_t page_offset = 0;
  bitmap->AllocatePage(page_offset);
  WritePhysicalPage(bitmap_physical_id, buf);

  // 更新DiskMetaPage
  meta_page->num_allocated_pages_++;
  meta_page->num_extents_ = std::max(meta_page->num_extents_, extent_id + 1);
  meta_page->extent_used_page_[extent_id]++;
  return extent_id * BITMAP_SIZE + page_offset; // 注意：逻辑页号
}

/**
 * TODO: Student Implement
 */
void DiskManager::DeAllocatePage(page_id_t logical_page_id) {
  if (IsPageFree(logical_page_id)) return;

  // 读取DiskMetaPage
  DiskFileMetaPage *meta_page = reinterpret_cast<DiskFileMetaPage *>(meta_data_);
  page_id_t page_offset = logical_page_id % BITMAP_SIZE;
  page_id_t bitmap_physical_id = 1 + logical_page_id / BITMAP_SIZE * (BITMAP_SIZE + 1); // 逻辑转换物理

  // 读取位图用于释放
  char buf[PAGE_SIZE];
  ReadPhysicalPage(bitmap_physical_id, buf);
  BitmapPage<PAGE_SIZE> *bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(buf);

  // 检查是否释放成功
  if (!bitmap->DeAllocatePage(page_offset)) {
    LOG(WARNING) << "Failed to deallocate page: " << logical_page_id;
    return;
  }

  // 更新DiskMetaPage
  meta_page->num_allocated_pages_--;
  meta_page->extent_used_page_[logical_page_id / BITMAP_SIZE]--;
  WritePhysicalPage(bitmap_physical_id, buf);
}

/**
 * TODO: Student Implement
 */
bool DiskManager::IsPageFree(page_id_t logical_page_id) {
  if (logical_page_id < 0 || logical_page_id > MAX_VALID_PAGE_ID) return false;
  page_id_t page_offset = logical_page_id % BITMAP_SIZE;
  page_id_t bitmap_physical_id = 1 + logical_page_id / BITMAP_SIZE * (BITMAP_SIZE + 1);

  char buf[PAGE_SIZE];
  ReadPhysicalPage(bitmap_physical_id, buf);
  BitmapPage<PAGE_SIZE> *bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(buf);
  return bitmap->IsPageFree(page_offset);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::MapPageId(page_id_t logical_page_id) {
  // metadata page: 1
  // bitmap page: logical_page_id / BITMAP_SIZE + 1
  // data page: logical_page_id
  return logical_page_id + logical_page_id / BITMAP_SIZE + 2; // metapage + bitmap + data
}

int DiskManager::GetFileSize(const std::string &file_name) {
  struct stat stat_buf;
  int rc = stat(file_name.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void DiskManager::ReadPhysicalPage(page_id_t physical_page_id, char *page_data) {
  int offset = physical_page_id * PAGE_SIZE;
  // check if read beyond file length
  if (offset >= GetFileSize(file_name_)) {
#ifdef ENABLE_BPM_DEBUG
    LOG(INFO) << "Read less than a page" << std::endl;
#endif
    memset(page_data, 0, PAGE_SIZE);
  } else {
    // set read cursor to offset
    db_io_.seekp(offset);
    db_io_.read(page_data, PAGE_SIZE);
    // if file ends before reading PAGE_SIZE
    int read_count = db_io_.gcount();
    if (read_count < PAGE_SIZE) {
#ifdef ENABLE_BPM_DEBUG
      LOG(INFO) << "Read less than a page" << std::endl;
#endif
      memset(page_data + read_count, 0, PAGE_SIZE - read_count);
    }
  }
}

void DiskManager::WritePhysicalPage(page_id_t physical_page_id, const char *page_data) {
  size_t offset = static_cast<size_t>(physical_page_id) * PAGE_SIZE;
  // set write cursor to offset
  db_io_.seekp(offset);
  db_io_.write(page_data, PAGE_SIZE);
  // check for I/O error
  if (db_io_.bad()) {
    LOG(ERROR) << "I/O error while writing";
    return;
  }
  // needs to flush to keep disk file in sync
  db_io_.flush();
}
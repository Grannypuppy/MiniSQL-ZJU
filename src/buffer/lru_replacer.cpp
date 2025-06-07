#include "buffer/lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages){capacity = num_pages;}

LRUReplacer::~LRUReplacer() = default;

/**
 * TODO: Student Implement
 */
bool LRUReplacer::Victim(frame_id_t *frame_id) {
    // 无可替换页
    if (lru_list_.size() == 0)
        return false;
    
    // back为最近最少使用的页
    *frame_id = lru_list_.back();
    buf.erase(*frame_id);
    lru_list_.pop_back();

    return true;
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Pin(frame_id_t frame_id) {
    auto it = buf.find(frame_id);
    
    // 未找到说明无法被pin
    if (it == buf.end())
        return ;

    lru_list_.erase(it->second);
    buf.erase(it);
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Unpin(frame_id_t frame_id) {
    auto it = buf.find(frame_id);

    // 无需unpin
    if (it != buf.end())
        return ;
    ASSERT(lru_list_.size() < capacity, "LRU list full"); 

    lru_list_.emplace_front(frame_id); // 插入头部，说明是最近使用的页
    buf[frame_id] = lru_list_.begin();
}

/**
 * TODO: Student Implement
 */
size_t LRUReplacer::Size() {
    return lru_list_.size();
}
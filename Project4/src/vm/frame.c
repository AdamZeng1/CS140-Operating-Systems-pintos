#include "vm/frame.h"
#include "threads/malloc.h"
#include "../threads/malloc.h"
#include "frame.h"
#include "../lib/debug.h"
#include "../threads/palloc.h"
#include "../lib/stdint.h"
#include "../userprog/pagedir.h"
#include "../threads/thread.h"

static void frame_init_single(uint32_t idx, enum palloc_flags flags, struct supp_pagedir_entry *user);
static struct frame* frame_get_frame(uint32_t idx);

static struct frame_map *frame_map;

/* needs the lock */
static struct frame* frame_get_frame(uint32_t idx){
  ASSERT(idx < frame_map->num_of_frames);
  return frame_map->frames + idx;
}
/* needs the lock */
static void frame_init_single(uint32_t idx, enum palloc_flags flags, struct supp_pagedir_entry *user){
  struct frame *f = frame_get_frame(idx);

  ASSERT(f->MAGIC != FRAME_MAGIC);
  f->MAGIC = FRAME_MAGIC;
  f->user = user;
  f->prohibit_cache = flags & PAL_PROHIBIT_CACHE;
}

void frame_map_init(int pages_cnt){
  static int install = 0;
  if(install++ > 1) PANIC("frame is installed more then once");
  frame_map = malloc(sizeof(struct frame_map));
  ASSERT(frame_map);
  frame_map->frames = malloc(sizeof(struct frame) * pages_cnt);
  ASSERT(frame_map->frames);

  lock_init(&frame_map->lock);
  frame_map->num_of_frames = pages_cnt;
}

void * frame_get_page(enum palloc_flags flags, struct supp_pagedir_entry *user) {
  ASSERT(!(flags & PAL_THROUGH_FRAME));
  ASSERT(flags & PAL_USER);

  flags |= PAL_THROUGH_FRAME;

  lock_acquire(&frame_map->lock);

  void *page = palloc_get_page(flags);
  uint32_t idx = palloc_page_to_idx(flags, page);
  frame_init_single(idx, flags, user);

  lock_release(&frame_map->lock);

  return page;
}

void frame_free_page (void *upage){
  lock_acquire(&frame_map->lock);

  void *kpage = pagedir_get_page(thread_current()->pagedir, upage);
  uint32_t idx = palloc_page_to_idx(PAL_USER | PAL_THROUGH_FRAME, kpage);
  ASSERT(idx != UINT32_MAX);
  struct frame *f = frame_get_frame(idx);
  f->MAGIC = -1;
  f->user = NULL;

  palloc_free_page(kpage);

  lock_release(&frame_map->lock);
}
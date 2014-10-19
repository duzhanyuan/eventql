/**
 * This file is part of the "FnordMetric" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <fnordmetric/sstable/binaryformat.h>
#include <fnordmetric/sstable/livesstable.h>
#include <fnordmetric/util/runtimeexception.h>

namespace fnord {
namespace sstable {

std::unique_ptr<LiveSSTable> LiveSSTable::create(
    io::File file,
    IndexProvider index_provider,
    void const* header,
    size_t header_size) {
  if (file.size() > 0) {
    RAISE(kIllegalStateError, "file size must be 0");
  }

  auto sstable = new LiveSSTable(std::move(file), index_provider.popIndexes());
  sstable->writeHeader(header, header_size);
  return std::unique_ptr<LiveSSTable>(sstable);
}

std::unique_ptr<LiveSSTable> LiveSSTable::reopen(
    io::File file,
    IndexProvider index_provider) {
  auto sstable = new LiveSSTable(std::move(file), index_provider.popIndexes());
  return std::unique_ptr<LiveSSTable>(sstable);
}

LiveSSTable::LiveSSTable(
    io::File&& file,
    std::vector<Index::IndexRef>&& indexes) :
    file_(std::move(file)),
    indexes_(std::move(indexes)),
    mmap_(new io::MmapPageManager(file_.fd(), file_.size(), 1)),
    header_size_(0),
    body_size_(0) {}

LiveSSTable::~LiveSSTable() {
}

// FIXPAUL lock
void LiveSSTable::appendRow(
    void const* key,
    size_t key_size,
    void const* data,
    size_t data_size) {
  // assert that key is monotonically increasing...

  size_t page_size = sizeof(BinaryFormat::RowHeader) + key_size + data_size;
  auto alloc = mmap_->allocPage(page_size);
  auto page = mmap_->getPage(alloc);

  auto header = page->structAt<BinaryFormat::RowHeader>(0);
  header->key_size = key_size;
  header->data_size = data_size;

  auto key_dst = page->structAt<void>(sizeof(BinaryFormat::RowHeader));
  memcpy(key_dst, key, key_size);

  auto data_dst = page->structAt<void>(
      sizeof(BinaryFormat::RowHeader) + key_size);
  memcpy(data_dst, data, data_size);

  page->sync();

  auto row_body_offset = body_size_;
  body_size_ += page_size;

  for (const auto& idx : indexes_) {
    idx->addRow(row_body_offset, key, key_size, data, data_size);
  }
}

void LiveSSTable::appendRow(
    const std::string& key,
    const std::string& value) {
  appendRow(key.data(), key.size(), value.data(), value.size());
}

// FIXPAUL lock
void LiveSSTable::writeHeader(void const* data, size_t size) {
  auto alloc = mmap_->allocPage(sizeof(BinaryFormat::FileHeader) + size);
  auto page = mmap_->getPage(alloc);

  if (alloc.offset != 0) {
    RAISE(kIllegalStateError, "header page offset must be 0");
  }

  auto header = page->structAt<BinaryFormat::FileHeader>(0);
  header->magic = BinaryFormat::kMagicBytes;
  header->body_size = 0;
  header->header_size = size;

  if (size > 0) {
    auto userdata = page->structAt<void>(sizeof(BinaryFormat::FileHeader));
    memcpy(userdata, data, size);
  }

  page->sync();
  header_size_ = alloc.size;
}

// FIXPAUL lock
void LiveSSTable::finalize() {
  auto page = mmap_->getPage(
      io::PageManager::Page(0, sizeof(BinaryFormat::FileHeader)));

  auto header = page->structAt<BinaryFormat::FileHeader>(0);
  header->body_size = body_size_;
}

// FIXPAUL lock
std::unique_ptr<Cursor> LiveSSTable::getCursor() {
  return std::unique_ptr<Cursor>(
      new LiveSSTable::Cursor(this, mmap_.get()));
}

// FIXPAUL lock
size_t LiveSSTable::bodySize() const {
  return body_size_;
}

size_t LiveSSTable::headerSize() const {
  return header_size_;
}

LiveSSTable::Cursor::Cursor(
    LiveSSTable* table,
    io::MmapPageManager* mmap) :
    table_(table),
    mmap_(mmap),
    pos_(0) {}

void LiveSSTable::Cursor::seekTo(size_t body_offset) {
  if (body_offset >= table_->bodySize()) {
    RAISE(kIndexError, "seekTo() out of bounds position");
  }

  pos_ = body_offset;
}

bool LiveSSTable::Cursor::next() {
  auto page = getPage();
  auto header = page->structAt<BinaryFormat::RowHeader>(0);

  size_t page_size = page->page_.size;
  size_t row_size = sizeof(BinaryFormat::RowHeader) + header->key_size +
      header->data_size;

  if (row_size > page_size) {
    RAISE(kIllegalStateError, "row exceeds page boundary");
  }

  if (row_size == page_size) {
    return false;
  } else {
    pos_ += row_size;
    return true;
  }
}

bool LiveSSTable::Cursor::valid() {
  return pos_ < table_->bodySize();
}

void LiveSSTable::Cursor::getKey(void** data, size_t* size) {
  auto page = getPage();
  size_t page_size = page->page_.size;

  auto header = page->structAt<BinaryFormat::RowHeader>(0);
  if (header->key_size == 0) {
    RAISE(kIllegalStateError, "empty key");
  }

  if (sizeof(BinaryFormat::RowHeader) + header->key_size > page_size) {
    RAISE(kIllegalStateError, "key exceeds page boundary");
  }

  *data = page->structAt<void>(sizeof(BinaryFormat::RowHeader));
  *size = header->key_size;
}

void LiveSSTable::Cursor::getData(void** data, size_t* size) {
  auto page = getPage();
  auto header = page->structAt<BinaryFormat::RowHeader>(0);

  size_t page_size = page->page_.size;
  size_t row_size = sizeof(BinaryFormat::RowHeader) + header->key_size +
      header->data_size;
  if (row_size > page_size) {
    RAISE(kIllegalStateError, "row exceeds page boundary");
  }

  *data = page->structAt<void>(
      sizeof(BinaryFormat::RowHeader) + header->key_size);
  *size = header->data_size;
}

std::unique_ptr<io::PageManager::PageRef> LiveSSTable::Cursor::getPage() {
  return mmap_->getPage(io::PageManager::Page(
      table_->headerSize() + pos_,
      table_->bodySize() - pos_));
}

}
}

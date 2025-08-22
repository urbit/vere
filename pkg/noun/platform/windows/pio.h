ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
  off_t orig = lseek(fd, 0, SEEK_CUR);
  if (orig == (off_t)-1) {
    return -1;
  }
  if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
    return -1;
  }
  ssize_t res = read(fd, buf, count);
  lseek(fd, orig, SEEK_SET);  // Restore original offset, ignore potential error
  return res;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
  off_t orig = lseek(fd, 0, SEEK_CUR);
  if (orig == (off_t)-1) {
    return -1;
  }
  if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
    return -1;
  }
  ssize_t res = write(fd, buf, count);
  lseek(fd, orig, SEEK_SET);  // Restore original offset, ignore potential error
  return res;
}

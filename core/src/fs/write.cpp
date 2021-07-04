#include <userver/fs/write.hpp>

#include <userver/engine/async.hpp>
#include <userver/fs/blocking/write.hpp>

namespace fs {

void CreateDirectories(engine::TaskProcessor& async_tp, std::string_view path,
                       boost::filesystem::perms perms) {
  engine::impl::Async(async_tp, [path, perms] {
    fs::blocking::CreateDirectories(path, perms);
  }).Get();
}

void CreateDirectories(engine::TaskProcessor& async_tp, std::string_view path) {
  engine::impl::Async(async_tp, [path] {
    fs::blocking::CreateDirectories(path);
  }).Get();
}

void RewriteFileContents(engine::TaskProcessor& async_tp,
                         const std::string& path, std::string_view contents) {
  engine::impl::Async(async_tp, &fs::blocking::RewriteFileContents, path,
                      contents)
      .Get();
}

void SyncDirectoryContents(engine::TaskProcessor& async_tp,
                           const std::string& path) {
  engine::impl::Async(async_tp, &fs::blocking::SyncDirectoryContents, path)
      .Get();
}

void Rename(engine::TaskProcessor& async_tp, const std::string& source,
            const std::string& destination) {
  engine::impl::Async(async_tp, &fs::blocking::Rename, source, destination)
      .Get();
}

void Chmod(engine::TaskProcessor& async_tp, const std::string& path,
           boost::filesystem::perms perms) {
  engine::impl::Async(async_tp, &fs::blocking::Chmod, path, perms).Get();
}

void RewriteFileContentsAtomically(engine::TaskProcessor& async_tp,
                                   const std::string& path,
                                   std::string_view contents,
                                   boost::filesystem::perms perms) {
  auto tmp_path = path + ".tmp";
  RewriteFileContents(async_tp, tmp_path, contents);

  boost::filesystem::path file_path(path);
  auto directory_path = file_path.parent_path();

  Rename(async_tp, tmp_path, path);
  SyncDirectoryContents(async_tp, directory_path.string());

  Chmod(async_tp, path, perms);
}

bool RemoveSingleFile(engine::TaskProcessor& async_tp,
                      const std::string& path) {
  return engine::impl::Async(async_tp, &fs::blocking::RemoveSingleFile, path)
      .Get();
}

}  // namespace fs

#pragma once
#include <se_export.hpp>
#include <string>

namespace AndroidNativeLibs {

/**
 * Android only lets an app load code that lives in its own private storage: external storage (where users put
 * files, including native extensions) is mounted `noexec`. This copies the library into the app's private
 * storage (unless an up to date copy is already there) and returns the path of the copy, which is what has to be
 * given to dlopen(). Returns an empty string if the copy failed.
 */
SE_EXPORT std::string stage(const std::string &path);

} // namespace AndroidNativeLibs

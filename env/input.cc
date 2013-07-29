// Copyright 2013
// Author: Christopher Van Arsdale

#include <string>
#include <vector>
#include "repobuild/env/input.h"
#include "common/strings/path.h"

using std::string;

namespace repobuild {

Input::Input() {
  current_path_ = strings::CurrentPath();
  root_dir_ = ".";
  full_root_dir_ = strings::JoinPath(current_path_, root_dir_);
  object_dir_ = "obj";
  source_dir_ = "src";
}

const std::vector<std::string>& Input::flags(const std::string& key) const {
  auto it = flags_.find(key);
  if (it == flags_.end()) {
    static std::vector<std::string> kEmpty;
    return kEmpty;
  }
  return it->second;
}



}  // namespace repobuild

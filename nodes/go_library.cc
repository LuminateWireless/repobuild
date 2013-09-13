// Copyright 2013
// Author: Christopher Van Arsdale

#include <set>
#include <string>
#include <vector>
#include "common/strings/strutil.h"
#include "common/strings/path.h"
#include "repobuild/env/input.h"
#include "repobuild/nodes/go_library.h"
#include "repobuild/reader/buildfile.h"

using std::vector;
using std::string;
using std::set;

namespace repobuild {

void GoLibraryNode::Parse(BuildFile* file, const BuildFileNode& input) {
  SimpleLibraryNode::Parse(file, input);

  // go_sources
  current_reader()->ParseRepeatedFiles("go_sources", &sources_);

  Init();
}

void GoLibraryNode::Init() {
  touchfile_ = Touchfile();
}

void GoLibraryNode::Set(const vector<Resource>& sources) {
  sources_ = sources;
  Init();
}

void GoLibraryNode::LocalWriteMakeInternal(bool write_user_target,
                                           Makefile* out) const {
  SimpleLibraryNode::LocalWriteMake(out);

  // Move all go code into a single directory.
  vector<Resource> symlinked_sources;
  for (const Resource& source : sources_) {
    Resource symlink = GoFileFor(source);
    symlinked_sources.push_back(symlink);
    string relative_to_symlink =
        strings::Repeat("../", strings::NumPathComponents(symlink.dirname()));
    string target = strings::JoinPath(relative_to_symlink, source.path());
    out->StartRule(symlink.path(), source.path());
    out->WriteCommand("mkdir -p " + symlink.dirname());
    out->WriteCommand("ln -s -f " + target + " " + symlink.path());
    out->FinishRule();
  }

  // Syntax check.
  out->StartRule(touchfile_.path(), strings::JoinAll(sources_, " "));
  out->WriteCommand("echo \"Compiling: " + target().full_path() + " (go)\"");
  if (!sources_.empty()) {
    for (const Resource& r : symlinked_sources) {
      out->WriteCommand(GoBuildPrefix() + " gofmt " +
                        r.path() + " > /dev/null");
    }
  }
  out->WriteCommand("mkdir -p " + Touchfile().dirname());
  out->WriteCommand("touch " + Touchfile().path());
  out->FinishRule();

  // User target.
  if (write_user_target) {
    ResourceFileSet deps;
    DependencyFiles(GOLANG, &deps);
    WriteBaseUserTarget(deps, out);
  }
}

void GoLibraryNode::LocalDependencyFiles(LanguageType lang,
                                         ResourceFileSet* files) const {
  SimpleLibraryNode::LocalDependencyFiles(lang, files);
  files->Add(touchfile_);
}

void GoLibraryNode::LocalObjectFiles(LanguageType lang,
                                     ResourceFileSet* files) const {
  for (const Resource& r : sources_) {
    files->Add(GoFileFor(r));
  }
}

Resource GoLibraryNode::GoFileFor(const Resource& r) const {
  // HACK, go annoys me.
  if (strings::HasPrefix(r.path(), "src/")) {
    return Resource::FromLocalPath(input().gofile_dir(), r.path());
  } else {
    return Resource::FromLocalPath(input().gofile_dir() + "/src", r.path());
  }
}

string GoLibraryNode::GoBuildPrefix() const {
  return MakefileEscape("GOPATH=$(pwd)/" + input().gofile_dir() + ":$GOPATH");
}

}  // namespace repobuild

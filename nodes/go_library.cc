// Copyright 2013
// Author: Christopher Van Arsdale

#include <set>
#include <string>
#include <vector>
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

  // Syntax check.
  string sources = strings::JoinAll(sources_, " ");
  out->StartRule(touchfile_.path(), sources);
  out->WriteCommand(
      "mkdir -p " + ObjectDir() +
      "; gofmt -e " + sources + " && touch " + Touchfile().path());
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

}  // namespace repobuild

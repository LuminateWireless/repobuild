// Copyright 2013
// Author: Christopher Van Arsdale

#ifndef _REPOBUILD_NODES_NODE_H__
#define _REPOBUILD_NODES_NODE_H__

#include <string>
#include <set>
#include <vector>
#include <memory>
#include "repobuild/env/target.h"

namespace repobuild {

class BuildFile;
class BuildFileNode;
class Input;

class Node {
 public:
  explicit Node(const TargetInfo& target,
                const Input& input)
      : target_(target),
        input_(&input),
        strict_file_mode_(true) {
  }
  virtual ~Node() {
    for (auto it : dependencies_) {
      delete it;
    }
    for (auto it : owned_subnodes_) {
      delete it;
    }
  }

  // Virtual interface.
  virtual void Parse(BuildFile* file, const BuildFileNode& input);
  virtual void WriteMakefile(const std::vector<const Node*>& all_deps,
                             std::string* out) const = 0;
  virtual void WriteMakeClean(std::string* out) const {}
  virtual void DependencyFiles(std::vector<std::string>* files) const {}
  virtual void ObjectFiles(std::vector<std::string>* files) const {}
  virtual void FinalOutputs(std::vector<std::string>* outputs) const {}
  virtual void LinkFlags(std::set<std::string>* flags) const {}
  virtual void CompileFlags(std::set<std::string>* flags) const {}

  // Accessors.
  const Input& input() const { return *input_; }
  const TargetInfo& target() const { return target_; }
  const std::vector<TargetInfo*> dependencies() const { return dependencies_; }

  // Mutators
  void AddDependency(const TargetInfo& other);
  void SetStrictFileMode(bool strict) { strict_file_mode_ = strict; }

  // Subnode handling.
  void ExtractSubnodes(std::vector<Node*>* nodes) {
    *nodes = subnodes_;
    owned_subnodes_.clear();
  }
  void AddSubNode(Node* node) {
    AddDependency(node->target());
    subnodes_.push_back(node);
    owned_subnodes_.push_back(node);
  }

 protected:
  // Helpers
  void ParseRepeatedString(const BuildFileNode& input,
                           const std::string& key,
                           std::vector<std::string>* output) const {
    ParseRepeatedString(input, key, false /* use root path */, output);
  }
  void ParseRepeatedString(const BuildFileNode& input,
                           const std::string& key,
                           bool relative_gendir,
                           std::vector<std::string>* output) const;
  void ParseRepeatedFiles(const BuildFileNode& input,
                          const std::string& key,
                          std::vector<std::string>* output) const;
  bool ParseStringField(const BuildFileNode& input,
                        const std::string& key,
                        std::string* field) const;
  bool ParseBoolField(const BuildFileNode& input,
                      const std::string& key,
                      bool* field) const;
  void CollectDependencies(const std::vector<const Node*>& all_deps,
                           std::set<std::string>* files) const;
  void CollectObjects(const std::vector<const Node*>& all_deps,
                      std::set<std::string>* files) const;
  void CollectLinkFlags(const std::vector<const Node*>& all_deps,
                        std::set<std::string>* flags) const;
  void CollectCompileFlags(const std::vector<const Node*>& all_deps,
                           std::set<std::string>* flags) const;
  std::string ParseSingleString(const std::string& input) const {
    return ParseSingleString(true, input);
  }
  std::string ParseSingleString(bool relative_gendir,
                                const std::string& input) const;
  std::string GenDir() const;
  std::string RelativeGenDir() const;
  std::string MakefileEscape(const std::string& str) const;

 private:
  TargetInfo target_;
  const Input* input_;
  std::vector<TargetInfo*> dependencies_;
  bool strict_file_mode_;

  std::vector<Node*> subnodes_, owned_subnodes_;
};

}  // namespace repobuild

#endif // _REPOBUILD_NODES_NODE_H__

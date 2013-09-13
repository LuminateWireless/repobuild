// Copyright 2013
// Author: Christopher Van Arsdale

#ifndef _REPOBUILD_NODES_NODE_H__
#define _REPOBUILD_NODES_NODE_H__

#include <list>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <utility>
#include <vector>
#include "repobuild/env/resource.h"
#include "repobuild/env/target.h"
#include "common/strings/strutil.h"

namespace repobuild {

class BuildFile;
class BuildFileNode;
class BuildFileNodeReader;
class Input;

class Makefile;

class Node {
 public:
  // TODO(cvanarsdale): This is hacky.
  enum LanguageType {
    C_LANG = 0,
    CPP = 1,
    JAVA = 2,
    PYTHON = 3,
    GOLANG = 4,
    NO_LANG = 5,
  };

  Node(const TargetInfo& target, const Input& input);
  virtual ~Node();

  // Initialization
  virtual void Parse(BuildFile* file, const BuildFileNode& input);

  // Makefile generation.
  void WriteMake(Makefile* out) const;
  void WriteMakeClean(Makefile* out) const;

  // Internal object/resource handling.
  void DependencyFiles(LanguageType lang, ResourceFileSet* files) const;
  void ObjectFiles(LanguageType lang, ResourceFileSet* files) const;
  void FinalOutputs(LanguageType lang, ResourceFileSet* outputs) const;

  // Flag inheritence
  void LinkFlags(LanguageType lang, std::set<std::string>* flags) const;
  void CompileFlags(LanguageType lang, std::set<std::string>* flags) const;
  void IncludeDirs(LanguageType lang, std::set<std::string>* dirs) const;
  void EnvVariables(LanguageType lang,
                    std::map<std::string, std::string>* vars) const;

  // Accessors.
  const Input& input() const { return *input_; }
  const TargetInfo& target() const { return target_; }
  const std::vector<TargetInfo> dep_targets() const { return dep_targets_; }
  const std::vector<Node*> dependencies() const { return dependencies_; }

  // Mutators
  void AddDependencyTarget(const TargetInfo& other);
  void SetStrictFileMode(bool strict) { strict_file_mode_ = strict; }

  // Called from outside:
  void AddDependencyNode(Node* dependency);

  // Subnode handling.
  void ExtractSubnodes(std::vector<Node*>* nodes) {
    for (Node* n : subnodes_) {
      nodes->push_back(n);
      n->ExtractSubnodes(nodes);
    }
    owned_subnodes_.clear();
  }
  void AddSubNode(Node* node) {
    AddDependencyTarget(node->target());
    subnodes_.push_back(node);
    owned_subnodes_.push_back(node);
  }

 protected:
  class MakeVariable {
   public:
    explicit MakeVariable(const std::string& name) : name_(name) {}
    ~MakeVariable() {}

    const std::string& name() const { return name_; }
    std::string ref_name() const {
      return name_.empty() ? "" : "$(" + name_ + ")";
    }
    void SetValue(const std::string& value) { SetCondition("", value, ""); }
    void SetCondition(const std::string& condition,
                      const std::string& if_val,
                      const std::string& else_val) {
      conditions_[condition] = std::make_pair(if_val, else_val);
    }
    void WriteMake(std::string* out) const;

   private:
    std::string name_;
    std::map<std::string, std::pair<std::string, std::string> > conditions_;
  };

  // The main thing to override.
  virtual void LocalWriteMake(Makefile* out) const = 0;
  virtual void LocalWriteMakeClean(Makefile* out) const {}
  virtual void LocalDependencyFiles(LanguageType lang,
                                    ResourceFileSet* files) const {}
  virtual void LocalObjectFiles(LanguageType lang,
                                ResourceFileSet* files) const {}
  virtual void LocalFinalOutputs(LanguageType lang,
                                 ResourceFileSet* outputs) const {}
  virtual void LocalLinkFlags(LanguageType lang,
                              std::set<std::string>* flags) const {}
  virtual void LocalCompileFlags(LanguageType lang,
                                 std::set<std::string>* flags) const {}
  virtual void LocalIncludeDirs(LanguageType lang,
                                std::set<std::string>* dirs) const {}
  virtual void LocalEnvVariables(
      LanguageType lang, 
      std::map<std::string, std::string>* vars) const;

  // Parsing helpers
  BuildFileNodeReader* NewBuildReader(const BuildFileNode& node) const;
  BuildFileNodeReader* current_reader() const { return build_reader_.get(); }

  // Directory helpers.
  std::string GenDir() const { return gen_dir_; }
  std::string RelativeGenDir() const { return relative_gen_dir_; }
  std::string ObjectDir() const { return obj_dir_; }
  std::string RelativeObjectDir() const { return relative_obj_dir_; }
  std::string SourceDir() const { return src_dir_; }
  std::string RelativeSourceDir() const { return relative_src_dir_; }
  std::string RelativeRootDir() const { return relative_root_dir_; }
  std::string StripSpecialDirs(const std::string& path) const;

  // Makefile helpers.
  std::string MakefileEscape(const std::string& str) const;
  Resource Touchfile(const std::string& suffix) const;
  Resource Touchfile() const { return Touchfile(""); }
  void WriteBaseUserTarget(const ResourceFileSet& deps,
                           Makefile* out) const;
  void WriteBaseUserTarget(Makefile* out) const {
    ResourceFileSet empty;
    WriteBaseUserTarget(empty, out);
  }
  void WriteVariables(std::string* out) const {
    for (auto const& it : make_variables_) {
      it.second->WriteMake(out);
    }
  }
  bool HasVariable(const std::string& name) const {
    return make_variables_.find(name) != make_variables_.end();
  }
  const MakeVariable& GetVariable(const std::string& name) const {
    static MakeVariable kEmpty("");
    const auto& it = make_variables_.find(name);
    if (it == make_variables_.end()) {
      return kEmpty;
    }
    return *it->second;
  }
  MakeVariable* MutableVariable(const std::string& name) {
    MakeVariable** var = &(make_variables_[name]);
    if (*var == NULL) {
      *var = new MakeVariable(name + "." + target().make_path());
    }
    return *var;
  }

  // Dependency helpers
  void InputDependencyFiles(LanguageType lang, ResourceFileSet* files) const;
  void InputObjectFiles(LanguageType lang, ResourceFileSet* files) const;
  void InputFinalOutputs(LanguageType lang, ResourceFileSet* outputs) const;
  void InputLinkFlags(LanguageType lang, std::set<std::string>* flags) const;
  void InputCompileFlags(LanguageType lang, std::set<std::string>* flags) const;
  void InputIncludeDirs(LanguageType lang, std::set<std::string>* dirs) const;
  void InputEnvVariables(LanguageType lang,
                         std::map<std::string, std::string>* vars) const;

  enum DependencyCollectionType {
    DEPENDENCY_FILES = 0,
    OBJECT_FILES = 1,
    FINAL_OUTPUTS = 2,
    LINK_FLAGS = 3,
    COMPILE_FLAGS = 4,
    INCLUDE_DIRS = 5,
    ENV_VARIABLES = 6
  };
  void CollectAllDependencies(DependencyCollectionType type,
                              LanguageType lang,
                              std::vector<Node*>* all_deps) const {
    std::set<Node*> all_deps_set(all_deps->begin(), all_deps->end());
    CollectAllDependencies(type, lang, &all_deps_set, all_deps);
  }
  void CollectAllDependencies(DependencyCollectionType type,
                              LanguageType lang,
                              std::set<Node*>* all_deps_set,
                              std::vector<Node*>* all_deps) const;
  virtual bool IncludeDependencies(DependencyCollectionType type,
                                   LanguageType lang) const {
    return true;
  }
  virtual bool IncludeChildDependency(DependencyCollectionType type,
                                      LanguageType lang,
                                      Node* node) const {
    return true;
  }

 private:
  // Input info.
  TargetInfo target_;
  const Input* input_;
  std::vector<TargetInfo> dep_targets_;
  std::string src_dir_, obj_dir_, gen_dir_;
  std::string relative_root_dir_, relative_src_dir_;
  std::string relative_obj_dir_, relative_gen_dir_;

  // Parsing info
  bool strict_file_mode_;
  std::unique_ptr<BuildFileNodeReader> build_reader_;
  std::map<std::string, std::string> env_variables_;

  // Subnode/variables/etc handling.
  std::vector<Node*> subnodes_, owned_subnodes_;
  std::vector<Node*> dependencies_;  // not owned.
  std::map<std::string, MakeVariable*> make_variables_;
};

// SimpleLibraryNode
//  A library that just collects dependencies.
class SimpleLibraryNode : public Node {
 public:
  SimpleLibraryNode(const TargetInfo& t, const Input& i) : Node(t, i) {}
  virtual ~SimpleLibraryNode() {}
  virtual void LocalWriteMake(Makefile* out) const {}
  virtual void LocalDependencyFiles(LanguageType lang,
                                    ResourceFileSet* files) const;
  virtual void LocalObjectFiles(LanguageType lang,
                                ResourceFileSet* files) const;

  // Alterative to Parse()
  virtual void LocalSet(LanguageType lang,const std::vector<Resource>& sources) {
    sources_ = sources;
  }

 protected:
  std::vector<Resource> sources_;
};

class Makefile {
 public:
  Makefile() :silent_(true) {}
  ~Makefile() {}

  // Options
  void SetSilent(bool silent) { silent_ = silent; }

  // Rules
  // TODO(cvanarsdale): Return a pointer to a MakefileRule.
  void StartRule(const std::string& rule) { StartRule(rule, ""); }
  void StartRule(const std::string& rule, const std::string& dependencies);
  void FinishRule();
  void WriteRule(const std::string& rule, const std::string& deps) {
    StartRule(rule, deps);
    FinishRule();
  }

  // Commands for rules.
  void WriteCommand(const std::string& command);
  void WriteCommandBestEffort(const std::string& command);

  std::string* mutable_out() { return &out_; }
  const std::string& out() const { return out_; }

  template <typename T>
  void append(const T& t) {
    out_.append(strings::StringPrint(t));
  }

 private:
  bool silent_;
  std::string out_;
};


}  // namespace repobuild

#endif // _REPOBUILD_NODES_NODE_H__

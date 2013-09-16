// Copyright 2013
// Author: Christopher Van Arsdale

#ifndef _REPOBUILD_NODES_MAKEFILE_H__
#define _REPOBUILD_NODES_MAKEFILE_H__

#include <string>
#include "common/strings/strutil.h"

namespace repobuild {

class Makefile {
 public:
  Makefile() :silent_(true) {}
  ~Makefile() {}

  // Options
  void SetSilent(bool silent) { silent_ = silent; }

  class Rule {
   public:
    Rule(const std::string& rule, const std::string& dependencies, bool silent);
    ~Rule() {}

    // Adding commands to our rule.
    void WriteCommand(const std::string& command);
    void WriteCommandBestEffort(const std::string& command);

    // Raw access.
    std::string* mutable_out() { return &out_; }
    const std::string& out() const { return out_; }

   private:
    bool silent_;
    std::string out_;
  };

  // Rules
  Rule* StartRule(const std::string& rule) { return StartRule(rule, ""); }
  Rule* StartRule(const std::string& rule, const std::string& dependencies);
  void FinishRule(Rule* rule);
  void WriteRule(const std::string& rule, const std::string& deps) {
    FinishRule(StartRule(rule, deps));
  }

  // Full access.
  std::string* mutable_out() { return &out_; }
  const std::string& out() const { return out_; }

  // Helper to make this easier.
  template <typename T>
  void append(const T& t) {
    out_.append(strings::StringPrint(t));
  }

 private:
  bool silent_;
  std::string out_;
};

}  // namespace repobuild

#endif  // _REPOBUILD_NODES_MAKEFILE_H__
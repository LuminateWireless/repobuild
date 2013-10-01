// Copyright 2013
// Author: Christopher Van Arsdale

#ifndef _REPOBUILD_NODES_JAVA_BINARY_H__
#define _REPOBUILD_NODES_JAVA_BINARY_H__

#include <string>
#include <vector>
#include "repobuild/nodes/node.h"
#include "repobuild/nodes/java_jar.h"

namespace repobuild {

class JavaBinaryNode : public JavaJarNode {
 public:
  JavaBinaryNode(const TargetInfo& t,
                 const Input& i)
      : JavaJarNode(t, i) {
  }
  virtual ~JavaBinaryNode() {}
  virtual void Parse(BuildFile* file, const BuildFileNode& input);
  virtual void LocalBinaries(LanguageType lang,
                             ResourceFileSet* outputs) const;
};

}  // namespace repobuild

# endif  // _REPOBUILD_NODES_JAVA_BINARY_H__

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace axiom::ai {

struct KnowledgeArticle {
  const char* id;
  const char* title;
  const char* tags;   // space-separated lowercase keywords
  const char* body;
};

class AIKnowledge {
 public:
  bool Begin();
  // Returns true if a local article matched; writes answer into dst.
  bool Lookup(const char* query, char* dst, size_t n) const;
  uint16_t ArticleCount() const;
  const KnowledgeArticle* ArticleAt(uint16_t i) const;
  bool ArticleBody(uint16_t i, char* dst, size_t n) const;

 private:
  int Score(const char* query, const KnowledgeArticle& a) const;
};

}  // namespace axiom::ai

#include "modules/ai/AIKnowledge.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "modules/ai/knowledge/kb_builtin.h"

namespace axiom::ai {

bool AIKnowledge::Begin() { return ArticleCount() > 0; }

uint16_t AIKnowledge::ArticleCount() const {
  uint16_t n = 0;
  BuiltinKnowledge(n);
  return n;
}

const KnowledgeArticle* AIKnowledge::ArticleAt(uint16_t i) const {
  uint16_t n = 0;
  const KnowledgeArticle* a = BuiltinKnowledge(n);
  if (i >= n) return nullptr;
  return &a[i];
}

bool AIKnowledge::ArticleBody(uint16_t i, char* dst, size_t n) const {
  const KnowledgeArticle* a = ArticleAt(i);
  if (!a || !dst || n == 0) return false;
  snprintf(dst, n, "%s\n\n%s", a->title, a->body);
  return true;
}

static void ToLowerCopy(const char* src, char* dst, size_t n) {
  size_t i = 0;
  for (; src[i] && i + 1 < n; ++i) {
    dst[i] = static_cast<char>(tolower(static_cast<unsigned char>(src[i])));
  }
  dst[i] = 0;
}

int AIKnowledge::Score(const char* query, const KnowledgeArticle& a) const {
  if (!query || !query[0]) return 0;
  char q[96];
  ToLowerCopy(query, q, sizeof(q));

  int score = 0;
  // Tokenize query by space
  char* save = nullptr;
  char buf[96];
  strncpy(buf, q, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  for (char* tok = strtok_r(buf, " ,.?!", &save); tok; tok = strtok_r(nullptr, " ,.?!", &save)) {
    if (strlen(tok) < 2) continue;
    if (strcasestr(a.tags, tok)) score += 3;
    if (strcasestr(a.title, tok)) score += 4;
    if (strcasestr(a.body, tok)) score += 1;
    if (strcasecmp(a.id, tok) == 0) score += 6;
  }
  return score;
}

bool AIKnowledge::Lookup(const char* query, char* dst, size_t n) const {
  if (!dst || n == 0) return false;
  dst[0] = 0;
  uint16_t count = 0;
  const KnowledgeArticle* arts = BuiltinKnowledge(count);
  int best = 0;
  int best_score = 0;
  for (uint16_t i = 0; i < count; ++i) {
    const int s = Score(query, arts[i]);
    if (s > best_score) {
      best_score = s;
      best = static_cast<int>(i);
    }
  }
  // Threshold: need at least one solid tag hit
  if (best_score < 3) return false;
  snprintf(dst, n, "[KB:%s] %s", arts[best].id, arts[best].body);
  return true;
}

}  // namespace axiom::ai

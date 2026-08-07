#pragma once

#include <stdint.h>

namespace axiom::ai {

struct KnowledgeArticle;

const KnowledgeArticle* BuiltinKnowledge(uint16_t& count);

}  // namespace axiom::ai

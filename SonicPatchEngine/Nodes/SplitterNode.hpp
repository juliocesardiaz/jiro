#pragma once
//
// SplitterNode.hpp
// Copies a single input to N identical outputs (a fan-out / "send" point).
//
#include "Node.hpp"

namespace sonicpatch {

class SplitterNode final : public Node {
public:
    explicit SplitterNode(NodeID id) noexcept : Node(NodeKind::Splitter, id) {}

    void prepare(const AudioFormat&) override {}
    void process(ProcessContext& ctx) override; // [RT]
};

} // namespace sonicpatch

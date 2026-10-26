#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace sc::onnx {

// ONNX Runtime (Microsoft, MIT), for translating on the computer: not part
// of SCANCORE but an optional download (see OnDevice), loaded the first time
// a model runs. Models run on the CPU.

// Loads the library at `path` once; false (and why) when it cannot.
bool load(const QString &path, QString *error = nullptr);
bool loaded();

// A tensor in or out: floats or 64-bit integers, row-major.
struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> floats;
    std::vector<int64_t> ints;
    bool isInt = false;

    static Tensor ofFloats(std::vector<int64_t> shape, std::vector<float> data) { return {std::move(shape), std::move(data), {}, false}; }
    static Tensor ofInts(std::vector<int64_t> shape, std::vector<int64_t> data) { return {std::move(shape), {}, std::move(data), true}; }
    int64_t count() const;
};

// One model. run() may be called from any thread at once.
class Session {
public:
    // Throws sc::Error when the model cannot be opened.
    explicit Session(const QString &model);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    std::vector<std::string> inputs() const { return m_inputs; }
    std::vector<std::string> outputs() const { return m_outputs; }
    // Inputs by name; returns the named outputs (all of them when empty).
    std::vector<Tensor> run(const std::vector<std::pair<std::string, Tensor>> &inputs, const std::vector<std::string> &outputs = {}) const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    std::vector<std::string> m_inputs, m_outputs;
};

} // namespace sc::onnx

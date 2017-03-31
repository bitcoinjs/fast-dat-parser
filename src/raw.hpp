#pragma once

// BLOCK_HEADER > stdout
template <typename Block>
struct dumpHeaders : public TransformBase<Block> {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		fwrite(block.header.begin(), 80, 1, stdout);
	}
};

// SCRIPT_LENGTH | SCRIPT > stdout
template <typename Block>
struct dumpScripts : public TransformBase<Block> {
	void operator() (const Block& block) {
		if (this->shouldSkip(block)) return;

		std::array<uint8_t, 4096> buffer;
		const auto maxScriptLength = buffer.size() - sizeof(uint16_t);

		auto transactions = block.transactions();
		while (not transactions.empty()) {
			const auto& transaction = transactions.front();

			for (const auto& input : transaction.inputs) {
				if (input.script.size() > maxScriptLength) continue;

				auto r = range(buffer);
				serial::put<uint16_t>(r, static_cast<uint16_t>(input.script.size()));
				r.put(input.script);
				fwrite(buffer.begin(), buffer.size() - r.size(), 1, stdout);
			}

			for (const auto& output : transaction.outputs) {
				if (output.script.size() > maxScriptLength) continue;

				auto r = range(buffer);
				serial::put<uint16_t>(r, static_cast<uint16_t>(output.script.size()));
				r.put(output.script);
				fwrite(buffer.begin(), buffer.size() - r.size(), 1, stdout);
			}

			transactions.popFront();
		}
	}
};

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

static bool isExecutableFile(const fs::path &path) {
	struct stat st{};
	if (stat(path.c_str(), &st) != 0) return false;
	if (!S_ISREG(st.st_mode)) return false;
	if (access(path.c_str(), X_OK) != 0) return false;
	return true;
}

static bool readFilePrefix(const fs::path &path, std::string &out, std::size_t bytes) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	out.resize(bytes);
	f.read(out.data(), static_cast<std::streamsize>(bytes));
	return f.gcount() == static_cast<std::streamsize>(bytes);
}

static bool isElfBinary(const fs::path &path) {
	std::string prefix;
	if (!readFilePrefix(path, prefix, 4)) return false;
	return prefix.size() >= 4 && static_cast<unsigned char>(prefix[0]) == 0x7f && prefix[1] == 'E' && prefix[2] == 'L' && prefix[3] == 'F';
}

static std::optional<std::string> which(const std::string &exe) {
	const char *pathEnv = ::getenv("PATH");
	if (!pathEnv) return std::nullopt;
	std::stringstream ss(pathEnv);
	std::string dir;
	while (std::getline(ss, dir, ':')) {
		fs::path candidate = fs::path(dir) / exe;
		if (fs::exists(candidate) && fs::is_regular_file(candidate) && access(candidate.c_str(), X_OK) == 0) {
			return candidate.string();
		}
	}
	return std::nullopt;
}

static int runCommand(const std::vector<std::string> &args, bool quiet = false) {
	std::string cmd;
	for (std::size_t i = 0; i < args.size(); ++i) {
		if (i) cmd += ' ';
		// naive escaping for spaces
		if (args[i].find(' ') != std::string::npos) {
			cmd += '"' + args[i] + '"';
		} else {
			cmd += args[i];
		}
	}
	if (!quiet) std::cerr << "[exec] " << cmd << "\n";
	int rc = std::system(cmd.c_str());
	if (rc == -1) return errno ? errno : 1;
	return WEXITSTATUS(rc);
}

static std::uintmax_t fileSize(const fs::path &p) {
	std::error_code ec;
	auto sz = fs::file_size(p, ec);
	return ec ? 0 : sz;
}

struct Tools {
	std::optional<std::string> strip;
	std::optional<std::string> objcopy;
	std::optional<std::string> upx;
	std::optional<std::string> patchelf;
	std::optional<std::string> sstrip;
};

static Tools detectTools() {
	Tools t;
	// Prefer LLVM tools on Termux; fallback to GNU
	t.strip = which("llvm-strip");
	if (!t.strip) t.strip = which("strip");
	// objcopy
	t.objcopy = which("llvm-objcopy");
	if (!t.objcopy) t.objcopy = which("objcopy");
	// upx is optional
	t.upx = which("upx");
	// patchelf and sstrip are optional
	t.patchelf = which("patchelf");
	t.sstrip = which("sstrip");
	return t;
}

static bool backupOnce(const fs::path &target) {
	fs::path backupPath = target;
	backupPath += ".bak";
	if (fs::exists(backupPath)) return true;
	std::error_code ec;
	fs::copy_file(target, backupPath, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		std::cerr << "Failed to create backup: " << ec.message() << "\n";
		return false;
	}
	return true;
}

static bool optimizeOnce(const fs::path &target, const Tools &tools) {
	bool anyShrank = false;
	std::uintmax_t sizeNow = fileSize(target);
	std::uintmax_t beforeStep = 0;

	auto tryStep = [&](const std::vector<std::string> &cmd) {
		beforeStep = sizeNow;
		int rc = runCommand(cmd, true);
		sizeNow = fileSize(target);
		if (rc == 0 && sizeNow < beforeStep) {
			anyShrank = true;
		}
	};

	// 1) Strip symbols (unneeded first, then all)
	if (tools.strip) {
		tryStep({*tools.strip, "--strip-unneeded", target.string()});
		tryStep({*tools.strip, "--strip-all", target.string()});
	}

	// 2) Remove debug info and common note/comment sections
	if (tools.objcopy) {
		tryStep({*tools.objcopy, "--strip-debug", target.string()});
		// Remove non-essential metadata sections
		tryStep({*tools.objcopy, "--remove-section=.comment", "--remove-section=.note", "--remove-section=.note.*", "--remove-section=.gnu_debuglink", target.string()});
		// Compress whatever debug sections may remain
		tryStep({*tools.objcopy, "--compress-debug-sections", target.string()});
	}

	// 3) Shrink RPATH if present
	if (tools.patchelf) {
		tryStep({*tools.patchelf, "--shrink-rpath", target.string()});
	}

	// 4) Super-strip (more aggressive)
	if (tools.sstrip) {
		tryStep({*tools.sstrip, target.string()});
	}

	// 5) Pack with UPX as final step
	if (tools.upx) {
		tryStep({*tools.upx, "--best", "--lzma", target.string()});
	}

	std::cerr << "Size: " << (fileSize(target) + 0) << " bytes\n";
	return anyShrank;
}

static void usage(const char *argv0) {
	std::cerr << "Usage: " << argv0 << " <program_path> -<times>\n";
	std::cerr << "\tPerforms multiple optimization passes over an ELF binary.\n";
	std::cerr << "\t<times> defaults to 1 if omitted. Example: " << argv0 << " ./a.out -2\n";
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	fs::path target = argv[1];
	int passes = 1;
	if (argc >= 3) {
		std::string a2 = argv[2];
		if (!a2.empty() && a2[0] == '-') {
			try {
				passes = std::stoi(a2.substr(1));
			} catch (...) {
				std::cerr << "Invalid optimization count: " << a2 << "\n";
				return 1;
			}
		} else {
			std::cerr << "Second argument must be -<times> (e.g., -2)\n";
			return 1;
		}
	}
	if (passes < 1) passes = 1;

	if (!fs::exists(target)) {
		std::cerr << "Target not found: " << target << "\n";
		return 1;
	}
	if (!isExecutableFile(target)) {
		std::cerr << "Target is not an executable file (or lacks execute permission): " << target << "\n";
		return 1;
	}
	if (!isElfBinary(target)) {
		std::cerr << "Target is not an ELF binary. Skipping.\n";
		return 1;
	}

	Tools tools = detectTools();
	if (!tools.strip && !tools.objcopy && !tools.upx) {
		std::cerr << "No optimization tools found in PATH (llvm-strip/strip, llvm-objcopy/objcopy, upx).\n";
		return 1;
	}

	if (!backupOnce(target)) return 1;

	for (int i = 1; i <= passes; ++i) {
		std::cerr << "Pass " << i << "/" << passes << "\n";
		bool shrank = optimizeOnce(target, tools);
		if (!shrank) {
			std::cerr << "No further changes; stopping early.\n";
			break;
		}
	}

	std::cerr << "Done.\n";
	return 0;
}


// generates a big list of permutations of configuration and build presets
// recognises by default msvc, clang, and gcc.
// for now only recognises x86 and x64

// for every compiler adds at a "native" preset. This preset just doesn't specify bitness/arch,
// and allows compiler to assume its'/systems's default

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static inline std::string trim(std::string s) {
	auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
	while (!s.empty() && is_ws((unsigned char)s.front())) s.erase(s.begin());
	while (!s.empty() && is_ws((unsigned char)s.back())) s.pop_back();
	return s;
}

static inline std::string toLower(std::string s) {
	for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
	return s;
}

static inline void appendFlags(std::string& base, const std::string& extra) {
	std::string e = trim(extra);
	if (e.empty()) return;
	if (base.empty()) base = e;
	else base += " " + e;
}

static inline std::string unquote(std::string v) {
	v = trim(v);
	if (v.size() >= 2) {
		char a = v.front();
		char b = v.back();
		if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
			v = v.substr(1, v.size() - 2);
		}
	}
	return v;
}

static std::vector<std::string> splitCommaRespectQuotes(const std::string& s) {
	std::vector<std::string> out;
	std::string cur;
	bool inQuotes = false;
	char quoteChar = 0;

	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
			if (!inQuotes) {
				inQuotes = true;
				quoteChar = c;
			}
			else if (quoteChar == c) {
				inQuotes = false;
				quoteChar = 0;
			}
			cur.push_back(c);
			continue;
		}
		if (c == ',' && !inQuotes) {
			out.push_back(trim(cur));
			cur.clear();
			continue;
		}
		cur.push_back(c);
	}
	if (!cur.empty()) out.push_back(trim(cur));
	return out;
}

// ------------------------ Minimal JSON builder ------------------------

struct J {
	enum class T { Null, Bool, Number, String, Array, Object };
	T t = T::Null;

	bool b = false;
	long long n = 0;
	std::string s;
	std::vector<J> a;
	std::vector<std::pair<std::string, J>> o; // preserve order

	static J null() { return J{}; }
	static J boolean(bool v) { J j; j.t = T::Bool; j.b = v; return j; }
	static J number(long long v) { J j; j.t = T::Number; j.n = v; return j; }
	static J str(std::string v) { J j; j.t = T::String; j.s = std::move(v); return j; }
	static J array() { J j; j.t = T::Array; return j; }
	static J object() { J j; j.t = T::Object; return j; }

	void push(J v) { a.push_back(std::move(v)); }

	void add(std::string key, J val) {
		// overwrite if exists, keep position
		for (auto& kv : o) {
			if (kv.first == key) { kv.second = std::move(val); return; }
		}
		o.emplace_back(std::move(key), std::move(val));
	}

	static std::string esc(const std::string& in) {
		std::ostringstream os;
		for (unsigned char c : in) {
			switch (c) {
				case '\\': os << "\\\\"; break;
				case '"':  os << "\\\""; break;
				case '\b': os << "\\b"; break;
				case '\f': os << "\\f"; break;
				case '\n': os << "\\n"; break;
				case '\r': os << "\\r"; break;
				case '\t': os << "\\t"; break;
				default:
					if (c < 0x20) {
						os << "\\u";
						const char* hex = "0123456789abcdef";
						os << '0' << '0' << hex[(c >> 4) & 0xF] << hex[c & 0xF];
					}
					else {
						os << (char)c;
					}
			}
		}
		return os.str();
	}

	void dump(std::ostream& os, int indent = 0) const {
		auto ind = [&](int k) { for (int i = 0; i < k; ++i) os << ' '; };

		switch (t) {
			case T::Null:   os << "null"; break;
			case T::Bool:   os << (b ? "true" : "false"); break;
			case T::Number: os << n; break;
			case T::String: os << '"' << esc(s) << '"'; break;

			case T::Array: {
				os << "[";
				if (!a.empty()) os << "\n";
				for (size_t i = 0; i < a.size(); ++i) {
					ind(indent + 2);
					a[i].dump(os, indent + 2);
					if (i + 1 != a.size()) os << ",";
					os << "\n";
				}
				if (!a.empty()) ind(indent);
				os << "]";
			} break;

			case T::Object: {
				os << "{";
				if (!o.empty()) os << "\n";
				for (size_t i = 0; i < o.size(); ++i) {
					ind(indent + 2);
					os << '"' << esc(o[i].first) << "\": ";
					o[i].second.dump(os, indent + 2);
					if (i + 1 != o.size()) os << ",";
					os << "\n";
				}
				if (!o.empty()) ind(indent);
				os << "}";
			} break;
		}
	}

	std::string dump(int indent = 0) const {
		std::ostringstream os;
		dump(os, indent);
		return os.str();
	}
};

// ------------------------ Preset generation ------------------------

struct CompilerSpec {
	std::string code;      // internal code name
	std::string label;     // display label
	std::string cc;
	std::string cxx;

	std::string cWarn, cxxWarn;
	std::string cDebug, cxxDebug;
	std::string cRelease, cxxRelease;
	std::string cMisc, cxxMisc;

	bool isMSVC = false;
	bool isGcc = false;
	bool isClang = false;
};

static std::string defaultCxxWarnFlags() {
	return "-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wundef "
		"-Wimplicit-fallthrough -Wnull-dereference -Wconversion "
		"-Wsign-conversion -Wcast-qual -Wcast-align -Wdouble-promotion";
}

static std::string defaultCWarnFlags() {
	// “Appropriate” C extras: strict/missing prototypes are common for pure C.
	return defaultCxxWarnFlags() + " -Wstrict-prototypes -Wmissing-prototypes";
}

static CompilerSpec makeDefaultCompiler(const std::string& code) {
	CompilerSpec c;
	c.code = code;
	std::string lc = toLower(code);

	if (lc == "msvc") {
		c.isMSVC = true;
		c.label = "MSVC";
		c.cc = "cl.exe";
		c.cxx = "cl.exe";
	}
	else if (lc == "gcc" || lc.rfind("gcc", 0) == 0) {
		c.isGcc = true;
		c.label = "GCC";
		c.cc = "gcc";
		c.cxx = "g++";
		c.cWarn = defaultCWarnFlags();
		c.cxxWarn = defaultCxxWarnFlags();
		c.cDebug = "-fno-omit-frame-pointer -fno-optimize-sibling-calls";
		c.cxxDebug = "-fno-omit-frame-pointer -fno-optimize-sibling-calls";
		c.cRelease = "-flto";
		c.cxxRelease = "-flto";
	}
	else if (lc == "clang" || lc.rfind("clang", 0) == 0) {
		c.isClang = true;
		c.label = "Clang";
		c.cc = "clang";
		c.cxx = "clang++";
		c.cWarn = defaultCWarnFlags();
		c.cxxWarn = defaultCxxWarnFlags();
		c.cDebug = "-fno-omit-frame-pointer -fno-optimize-sibling-calls";
		c.cxxDebug = "-fno-omit-frame-pointer -fno-optimize-sibling-calls";
		c.cRelease = "-flto";
		c.cxxRelease = "-flto";
	}
	else {
		// Unknown/custom compiler: keep minimal defaults.
		c.label = code;
		c.cc = "cc";
		c.cxx = "c++";
	}

	return c;
}

struct FlagEdit {
	bool append = false;
	std::string value;
};

static void applyFlagEdit(std::string& dst, const FlagEdit& e) {
	if (!e.append) dst = trim(e.value);
	else appendFlags(dst, e.value);
}

static std::optional<std::string> getArgValue(const std::string& arg, const std::string& prefix) {
	if (arg.rfind(prefix, 0) == 0) return arg.substr(prefix.size());
	return std::nullopt;
}

static CompilerSpec parseCompilerArg(const std::string& raw) {
	// raw: <code>[k=v,k+=v,...]  (bracket part optional)
	auto lb = raw.find('[');
	std::string code = (lb == std::string::npos) ? raw : raw.substr(0, lb);
	code = trim(code);

	CompilerSpec c = makeDefaultCompiler(code);

	if (lb == std::string::npos) return c;
	auto rb = raw.rfind(']');
	if (rb == std::string::npos || rb < lb) return c;

	std::string inside = raw.substr(lb + 1, rb - lb - 1);
	auto items = splitCommaRespectQuotes(inside);

	for (const auto& it0 : items) {
		std::string it = trim(it0);
		if (it.empty()) continue;

		bool isAppend = false;
		size_t pos = it.find("+=");
		if (pos != std::string::npos) {
			isAppend = true;
		}
		else {
			pos = it.find('=');
		}
		if (pos == std::string::npos) continue;

		std::string key = trim(it.substr(0, pos));
		std::string val = trim(it.substr(pos + (isAppend ? 2 : 1)));
		val = unquote(val);

		if (key == "CC") { c.cc = val; continue; }
		if (key == "CXX") { c.cxx = val; continue; }

		FlagEdit e{ isAppend, val };
		if (key == "cwarn") applyFlagEdit(c.cWarn, e);
		else if (key == "cxxwarn") applyFlagEdit(c.cxxWarn, e);
		else if (key == "cdebug") applyFlagEdit(c.cDebug, e);
		else if (key == "cxxdebug") applyFlagEdit(c.cxxDebug, e);
		else if (key == "crelease") applyFlagEdit(c.cRelease, e);
		else if (key == "cxxrelease") applyFlagEdit(c.cxxRelease, e);
		else if (key == "cflags") applyFlagEdit(c.cMisc, e);
		else if (key == "cxxflags") applyFlagEdit(c.cxxMisc, e);
	}

	// Recompute type flags if user chose a different code that still matches:
	// (Keep the original detection based on c.code, not overwritten.)
	return c;
}

static std::vector<std::string> parseArchList(std::string v) {
	v = trim(v);
	if (v.size() >= 2 && v.front() == '[' && v.back() == ']') {
		v = v.substr(1, v.size() - 2);
	}
	auto parts = splitCommaRespectQuotes(v);
	std::vector<std::string> out;
	for (auto p : parts) {
		p = toLower(trim(p));
		if (p == "x86" || p == "x64") out.push_back(p);
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
	return out;
}

static J makeWindowsCondition() {
	J cond = J::object();
	cond.add("type", J::str("equals"));
	cond.add("lhs", J::str("${hostSystemName}"));
	cond.add("rhs", J::str("Windows"));
	return cond;
}

static J makeBasePreset(const CompilerSpec& c,
	const std::string& cFlagsInit,
	const std::string& cxxFlagsInit) {
	J p = J::object();
	p.add("name", J::str(c.code + "-base"));
	p.add("hidden", J::boolean(true));
	p.add("generator", J::str("Ninja"));
	p.add("binaryDir", J::str("${sourceDir}/out/build/${presetName}"));
	p.add("installDir", J::str("${sourceDir}/out/install/${presetName}"));

	J cache = J::object();
	cache.add("CMAKE_C_COMPILER", J::str(c.cc));
	cache.add("CMAKE_CXX_COMPILER", J::str(c.cxx));
	cache.add("CMAKE_C_FLAGS_INIT", J::str(cFlagsInit));
	cache.add("CMAKE_CXX_FLAGS_INIT", J::str(cxxFlagsInit));
	p.add("cacheVariables", std::move(cache));

	J env = J::object();
	env.add("PROJ_C_WARN_FLAGS", J::str(c.cWarn));
	env.add("PROJ_CXX_WARN_FLAGS", J::str(c.cxxWarn));
	env.add("PROJ_C_DEBUG_FLAGS", J::str(""));
	env.add("PROJ_CXX_DEBUG_FLAGS", J::str(""));
	env.add("PROJ_C_RELEASE_FLAGS", J::str(""));
	env.add("PROJ_CXX_RELEASE_FLAGS", J::str(""));
	env.add("PROJ_C_MISC_FLAGS", J::str(c.cMisc));
	env.add("PROJ_CXX_MISC_FLAGS", J::str(c.cxxMisc));
	env.add("PROJ_C_ARCH_BITS", J::str(""));
	env.add("PROJ_CXX_ARCH_BITS", J::str(""));
	env.add("PROJ_C_TOOL_FLAGS", J::str(""));
	env.add("PROJ_CXX_TOOL_FLAGS", J::str(""));
	p.add("environment", std::move(env));

	if (c.isMSVC) {
		p.add("condition", makeWindowsCondition());
	}

	return p;
}

static J makeArchObject(const CompilerSpec& c, const std::string& arch) {
	// For MSVC, CMake commonly uses "Win32" for x86 platform.
	std::string val = arch;
	if (c.isMSVC && arch == "x86") val = "Win32";
	if (c.isMSVC && arch == "x64") val = "x64";

	J a = J::object();
	a.add("strategy", J::str("external"));
	a.add("value", J::str(val));
	return a;
}

static J makeConfigurePreset(const CompilerSpec& c,
	const std::string& name,
	const std::string& displayName,
	const std::string& inherits,
	const std::string& buildType,
	const std::optional<std::string>& arch,
	const std::map<std::string, std::string>& envOverrides) {
	J p = J::object();
	p.add("name", J::str(name));
	p.add("displayName", J::str(displayName));
	p.add("inherits", J::str(inherits));

	J cache = J::object();
	cache.add("CMAKE_BUILD_TYPE", J::str(buildType));
	p.add("cacheVariables", std::move(cache));

	if (arch.has_value()) {
		p.add("architecture", makeArchObject(c, *arch));
	}

	if (!envOverrides.empty()) {
		J env = J::object();
		for (auto& kv : envOverrides) env.add(kv.first, J::str(kv.second));
		p.add("environment", std::move(env));
	}

	if (c.isMSVC) {
		p.add("condition", makeWindowsCondition());
	}

	return p;
}

static std::string compilerLabel(const CompilerSpec& c) {
	return c.label.empty() ? c.code : c.label;
}

static std::string capFirst(std::string s) {
	if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
	return s;
}

struct NonHiddenPresetInfo {
	std::string name;
	std::string displayName;
};

static void addPerCompilerPresets(const CompilerSpec& c,
	const std::vector<std::string>& archs,
	std::vector<J>& configurePresets,
	std::vector<NonHiddenPresetInfo>& nonHidden) {
	const std::string baseName = c.code + "-base";
	const std::string label = compilerLabel(c);

	auto disp = [&](const std::string& archName, const std::string& cfg,
		const std::string& suffix) {
			std::string d = "(" + label + ") " + archName + " " + cfg;
			if (!suffix.empty()) d += " " + suffix;
			return d;
		};

	bool wantX86 = std::find(archs.begin(), archs.end(), "x86") != archs.end();
	bool wantX64 = std::find(archs.begin(), archs.end(), "x64") != archs.end();

	// Native Debug/Release
	{
		std::string n = c.code + "-native-debug";
		auto p = makeConfigurePreset(
			c, n, disp("Native", "Debug", ""), baseName, "Debug", std::nullopt,
			{
			  {"PROJ_C_DEBUG_FLAGS", c.cDebug},
			  {"PROJ_CXX_DEBUG_FLAGS", c.cxxDebug}
			}
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ n, disp("Native", "Debug", "") });
	}
	{
		std::string n = c.code + "-native-release";
		auto p = makeConfigurePreset(
			c, n, disp("Native", "Release", ""), baseName, "Release", std::nullopt,
			{
			  {"PROJ_C_RELEASE_FLAGS", c.cRelease},
			  {"PROJ_CXX_RELEASE_FLAGS", c.cxxRelease}
			}
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ n, disp("Native", "Release", "") });
	}

	// GCC Debug Analyzer
	if (c.isGcc) {
		std::string n = c.code + "-native-debug-analyzer";
		auto p = makeConfigurePreset(
			c, n, disp("Native", "Debug", "(Analyzer)"), c.code + "-native-debug", "Debug",
			std::nullopt,
			{
			  {"PROJ_C_TOOL_FLAGS", "-fanalyzer"},
			  {"PROJ_CXX_TOOL_FLAGS", "-fanalyzer"}
			}
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ n, disp("Native", "Debug", "(Analyzer)") });
	}

	// GCC/Clang Release Tuned
	if (c.isGcc || c.isClang) {
		std::string n = c.code + "-native-release-tuned";
		auto p = makeConfigurePreset(
			c, n, disp("Native", "Release", "(Tuned)"), c.code + "-native-release", "Release",
			std::nullopt,
			{
			  {"PROJ_C_TOOL_FLAGS", "-march=native -mtune=native"},
			  {"PROJ_CXX_TOOL_FLAGS", "-march=native -mtune=native"}
			}
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ n, disp("Native", "Release", "(Tuned)") });
	}

	auto archBits = [&](const std::string& a) -> std::map<std::string, std::string> {
		// Only apply -m32/-m64 to GCC/Clang.
		if (!(c.isGcc || c.isClang)) return {};
		if (a == "x86") return { {"PROJ_C_ARCH_BITS", "-m32"}, {"PROJ_CXX_ARCH_BITS", "-m32"} };
		if (a == "x64") return { {"PROJ_C_ARCH_BITS", "-m64"}, {"PROJ_CXX_ARCH_BITS", "-m64"} };
		return {};
		};

	// Arch-specific Debug/Release (inherits strategy rules described)
	auto addArchCfg = [&](const std::string& a, const std::string& cfg) {
		const bool both = wantX86 && wantX64;
		const std::string cfgLower = toLower(cfg);
		const std::string base = c.code + "-native-" + cfgLower;

		std::string inheritFrom;
		if (a == "x64") inheritFrom = base;
		else if (a == "x86") {
			if (both) inheritFrom = c.code + "-x64-" + cfgLower;
			else inheritFrom = base;
		}
		else {
			inheritFrom = base;
		}

		std::string name = c.code + "-" + a + "-" + cfgLower;
		auto env = archBits(a);
		auto p = makeConfigurePreset(
			c, name, disp(a, cfg, ""), inheritFrom, cfg, a, env
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ name, disp(a, cfg, "") });
		};

	if (wantX64) { addArchCfg("x64", "Debug"); addArchCfg("x64", "Release"); }
	if (wantX86) { addArchCfg("x86", "Debug"); addArchCfg("x86", "Release"); }

	// Arch analyzer (GCC)
	auto addArchAnalyzer = [&](const std::string& a) {
		if (!c.isGcc) return;
		bool both = wantX86 && wantX64;

		std::string inheritFrom;
		if (a == "x64") inheritFrom = c.code + "-native-debug-analyzer";
		else if (a == "x86") {
			if (both) inheritFrom = c.code + "-x64-debug-analyzer";
			else inheritFrom = c.code + "-native-debug-analyzer";
		}
		else inheritFrom = c.code + "-native-debug-analyzer";

		std::string name = c.code + "-" + a + "-debug-analyzer";

		// tool flags inherited from analyzer base; only need arch bits here
		auto env = archBits(a);
		auto p = makeConfigurePreset(
			c, name, disp(a, "Debug", "(Analyzer)"), inheritFrom, "Debug", a, env
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ name, disp(a, "Debug", "(Analyzer)") });
		};

	if (wantX64) addArchAnalyzer("x64");
	if (wantX86) addArchAnalyzer("x86");

	// Arch tuned (GCC/Clang)
	auto addArchTuned = [&](const std::string& a) {
		if (!(c.isGcc || c.isClang)) return;
		bool both = wantX86 && wantX64;

		std::string inheritFrom;
		if (a == "x64") inheritFrom = c.code + "-native-release-tuned";
		else if (a == "x86") {
			if (both) inheritFrom = c.code + "-x64-release-tuned";
			else inheritFrom = c.code + "-native-release-tuned";
		}
		else inheritFrom = c.code + "-native-release-tuned";

		std::string name = c.code + "-" + a + "-release-tuned";

		auto env = archBits(a);
		auto p = makeConfigurePreset(
			c, name, disp(a, "Release", "(Tuned)"), inheritFrom, "Release", a, env
		);
		configurePresets.push_back(p);
		nonHidden.push_back({ name, disp(a, "Release", "(Tuned)") });
		};

	if (wantX64) addArchTuned("x64");
	if (wantX86) addArchTuned("x86");
}

static J makeBuildPreset(const std::string& name,
	const std::string& displayName,
	const std::string& configurePreset,
	bool cleanFirst,
	const std::optional<std::vector<std::string>>& targets) {
	J p = J::object();
	p.add("name", J::str(name));
	p.add("displayName", J::str(displayName));
	p.add("configurePreset", J::str(configurePreset));
	if (cleanFirst) p.add("cleanFirst", J::boolean(true));
	if (targets.has_value()) {
		J t = J::array();
		for (const auto& x : *targets) t.push(J::str(x));
		p.add("targets", std::move(t));
	}
	return p;
}

int main(int argc, char** argv) {
	std::vector<CompilerSpec> compilers;
	std::vector<std::string> archs;
	std::string outPath;
	std::string globalCFlags, globalCxxFlags;
	int presetVersion = 6;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		if (arg == "--help" || arg == "-h") {
			std::cout
				<< "CMakePresets generator\n"
				<< "Usage:\n"
				<< "  presetgen [--out=FILE] [--arch=x86,x64] [--cflags=...] [--cxxflags=...]\n"
				<< "           [--preset-version=N] [--compiler=spec]...\n\n"
				<< "Compiler spec format:\n"
				<< "  --compiler=<code>[CC=...,CXX=...,cwarn=\"...\",cwarn+=\"...\",cxxwarn=...,\n"
				<< "                   cdebug=...,cxxdebug=...,crelease=...,cxxrelease=...,\n"
				<< "                   cflags=...,cxxflags=...]\n";
			return 0;
		}

		if (auto v = getArgValue(arg, "--out=")) { outPath = *v; continue; }
		if (auto v = getArgValue(arg, "--arch=")) { archs = parseArchList(*v); continue; }
		if (auto v = getArgValue(arg, "--cflags=")) { globalCFlags = unquote(*v); continue; }
		if (auto v = getArgValue(arg, "--cxxflags=")) { globalCxxFlags = unquote(*v); continue; }

		if (auto v = getArgValue(arg, "--compiler=")) {
			compilers.push_back(parseCompilerArg(*v));
			continue;
		}

		std::cerr << "Unknown argument: " << arg << "\n(use --help)\n";
		return 2;
	}

	if (compilers.empty()) {
		compilers.push_back(makeDefaultCompiler("msvc"));
		compilers.push_back(makeDefaultCompiler("clang"));
		compilers.push_back(makeDefaultCompiler("gcc"));
	}

	// Apply global misc flags (append to each compiler misc)
	for (auto& c : compilers) {
		appendFlags(c.cMisc, globalCFlags);
		appendFlags(c.cxxMisc, globalCxxFlags);
	}

	const std::string cFlagsInit =
		"$env{PROJ_C_WARN_FLAGS} $env{PROJ_C_DEBUG_FLAGS} $env{PROJ_C_RELEASE_FLAGS} "
		"$env{PROJ_C_MISC_FLAGS} $env{PROJ_C_ARCH_BITS} $env{PROJ_C_TOOL_FLAGS}";
	const std::string cxxFlagsInit =
		"$env{PROJ_CXX_WARN_FLAGS} $env{PROJ_CXX_DEBUG_FLAGS} $env{PROJ_CXX_RELEASE_FLAGS} "
		"$env{PROJ_CXX_MISC_FLAGS} $env{PROJ_CXX_ARCH_BITS} $env{PROJ_CXX_TOOL_FLAGS}";

	std::vector<J> configurePresets;
	configurePresets.reserve(256);

	// Base presets first (always at the top)
	for (const auto& c : compilers) {
		configurePresets.push_back(makeBasePreset(c, cFlagsInit, cxxFlagsInit));
	}

	std::vector<NonHiddenPresetInfo> nonHidden;
	nonHidden.reserve(256);

	// Actual presets
	for (const auto& c : compilers) {
		addPerCompilerPresets(c, archs, configurePresets, nonHidden);
	}

	// Build presets for every non-hidden configure preset
	std::vector<J> buildPresets;
	buildPresets.reserve(nonHidden.size() * 2);

	for (const auto& cfg : nonHidden) {
		//buildPresets.push_back(
		//	makeBuildPreset(cfg.name + "-build",
		//		"(Build) " + cfg.displayName,
		//		cfg.name,
		//		false,
		//		std::nullopt));
		//
		//buildPresets.push_back(
		//	makeBuildPreset(cfg.name + "-rebuild",
		//		"(Rebuild) " + cfg.displayName,
		//		cfg.name,
		//		true,
		//		std::nullopt));

		buildPresets.push_back(
			makeBuildPreset(cfg.name,
				cfg.displayName,
				cfg.name,
				false,
				std::nullopt));
	}

	// Root
	J root = J::object();
	root.add("version", J::number(3));

	J cmr = J::object();
	cmr.add("major", J::number(3));
	cmr.add("minor", J::number(21));
	cmr.add("patch", J::number(0));
	root.add("cmakeMinimumRequired", std::move(cmr));

	J cfgArr = J::array();
	for (auto& p : configurePresets) cfgArr.push(std::move(p));
	root.add("configurePresets", std::move(cfgArr));

	J bldArr = J::array();
	for (auto& p : buildPresets) bldArr.push(std::move(p));
	root.add("buildPresets", std::move(bldArr));

	const std::string json = root.dump(0) + "\n";

	if (!outPath.empty()) {
		std::ofstream f(outPath, std::ios::binary);
		if (!f) {
			std::cerr << "Failed to open output file: " << outPath << "\n";
			return 3;
		}
		f << json;
	}
	else {
		std::cout << json;
	}

	return 0;
}
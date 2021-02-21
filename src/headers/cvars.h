#pragma once

#include "pch.h"

namespace Raekor {

typedef std::variant<int, float, std::string> cvar_t;

template<class Archive> 
void serialize(Archive& archive, cvar_t& cvar) { 
    archive(cvar);
}

class ConVars {
private:
    struct GetVisitor {
        template<typename T>
        std::string operator()(T& value) {
            return std::to_string(value);
        }

        template<>
        std::string operator()(std::string& value) {
            return value;
        }
    };


    struct SetVisitor {
        SetVisitor(const std::string& value) : value(value) {}

        template<typename T> bool operator()(T& cvar) {
            return false;
        }

        template<> bool operator()(std::string& cvar) {
            cvar = value;
            return true;
        }

        template<> bool operator()(int& cvar) {
            cvar = std::stoi(value);
            return true;
        }

        template<> bool operator()(float& cvar) {
            cvar = std::stof(value);
            return true;
        }

        const std::string& value;
    };

public:
    ConVars() {
        std::ifstream stream("cvars.json");
        if (!stream.is_open()) return;

        cereal::JSONInputArchive archive(stream);
        archive(cvars);
    }

    ~ConVars() {
        std::ofstream stream("cvars.json");
        cereal::JSONOutputArchive archive(stream);
        archive(cvars);
    }

    template<typename T>
    [[nodiscard]] static T& create(const std::string& name, T value) {
        if (singleton->cvars.find(name) == singleton->cvars.end()) {
            singleton->cvars[name] = value;
        }

        return std::get<T>(singleton->cvars[name]);
    }

    static std::string get(const std::string& name) {
        try {
            GetVisitor visitor;
            return std::visit(visitor, singleton->cvars[name]);
        } catch (std::exception& e) {
            UNREFERENCED_PARAMETER(e);
            return {};
        }
    }

    static bool set(std::string name, const std::string& value) {
        if (singleton->cvars.find(name) == singleton->cvars.end()) {
            return false;
        }

        try {
            SetVisitor visitor(value);
            return std::visit(visitor, singleton->cvars[name]);
        } catch (const std::exception& e) {
            UNREFERENCED_PARAMETER(e);
            return false;
        }
    }

    inline auto end()    { return cvars.end();   }
    inline auto begin()  { return cvars.begin(); }
    inline size_t size() { return cvars.size();  }

    static ConVars& getIterable() { return *singleton.get(); }

private:
    std::unordered_map<std::string, cvar_t> cvars;

private:
    inline static std::unique_ptr<ConVars> singleton = std::make_unique<ConVars>();
};

} // raekor

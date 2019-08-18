#pragma once

#include "model.h"

namespace Raekor {

class Scene {
public:
    Scene() {}

    void add(const std::string& name);
    void set_mesh(const std::string& name, const std::string& path);
    void set_texture(const std::string& name, const std::string& path);
    void remove(const std::string& name);

    inline const bool empty() const { return models.empty(); }
    inline void clear() { models.clear(); }

    // iterators provided for nice user level code, see app.ccp for examples
    typedef std::unordered_map<std::string, Model>::iterator iterator;
    typedef std::unordered_map<std::string, Model>::const_iterator const_iterator;
    
    inline iterator begin() { return models.begin(); }
    inline iterator end() { return models.end(); }
    inline const_iterator begin() const { return models.begin(); }
    inline const_iterator end() const { return models.end(); }

    inline iterator operator[] (const char* name) { return models.find(name); }
    inline const_iterator operator[] (const char* name) const { return models.find(name); }

private:
    std::unordered_map<std::string, Model> models;
};

}
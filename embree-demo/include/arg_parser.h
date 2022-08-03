#pragma once

template <typename T>
class Singleton {
public:
    static T &getInstance() {
        static T instance{_{}};
        return instance;
    }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton) = delete;

protected:
    struct _ {};
    Singleton() = default;
};

class ArgParser : public Singleton<ArgParser> {
public:
    const char *input_filename = "meshes/test_sphere.ply";
    const char *output_filename = "DF_OUTPUT.ply";
    float voxel_density = 0.2;
    float DF_resolution_scale = 2.0; // per mesh in ue5
    float display_distance = 0.0f;
    bool outside_only = false;
    bool sample_mode = false;
    bool parallel = false;

    ArgParser(_) {};
    void parseCommandLine(int argc, const char *argv[]);
};
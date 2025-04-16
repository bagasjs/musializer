// Copied from nob_win64_msvc
#define MUSIALIZER_TARGET_NAME "win64-clang"

#define CONSOLE_MODE

bool build_musializer(void)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    cmd.count = 0;
        nob_cmd_append(&cmd, "llvm-rc");
        nob_cmd_append(&cmd, "/fo", "./build/musializer.res");
        nob_cmd_append(&cmd, "./src/musializer.rc");
        // NOTE: Do not change the order of commandline arguments to rc. Their argparser is weird.
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);

#ifdef MUSIALIZER_HOTRELOAD
    procs.count = 0;
            cmd.count = 0;
                nob_cmd_append(&cmd, "clang");
                nob_cmd_append(&cmd, "-mwindows", "-Wall", "-Wextra", "-ggdb");
                nob_cmd_append(&cmd, "-I.");
                nob_cmd_append(&cmd, "-I"RAYLIB_SRC_FOLDER);
                nob_cmd_append(&cmd, "-fPIC", "-shared");
                nob_cmd_append(&cmd, "-static-libgcc");
                nob_cmd_append(&cmd, "-o", "./build/libplug.dll");
                nob_cmd_append(&cmd,
                    "./src/plug.c",
                    "./src/ffmpeg_windows.c",
                    "./thirdparty/tinyfiledialogs.c");
                nob_cmd_append(&cmd,
                    "-L./build",
                    "-l:raylib.dll");
                nob_cmd_append(&cmd, "-lwinmm", "-lgdi32", "-lole32");
            nob_da_append(&procs, nob_cmd_run_async(cmd));

            cmd.count = 0;
                nob_cmd_append(&cmd, "clang");
                nob_cmd_append(&cmd, "-mwindows", "-Wall", "-Wextra", "-ggdb");
                nob_cmd_append(&cmd, "-I.");
                nob_cmd_append(&cmd, "-I"RAYLIB_SRC_FOLDER);
                nob_cmd_append(&cmd, "-o", "./build/musializer.exe");
                nob_cmd_append(&cmd,
                    "./src/musializer.c",
                    "./src/hotreload_windows.c");
                nob_cmd_append(&cmd,
                    "-Wl,-rpath=./build/",
                    "-Wl,-rpath=./",
                    nob_temp_sprintf("-Wl,-rpath=./build/raylib/%s", MUSIALIZER_TARGET_NAME),
                    // NOTE: just in case somebody wants to run musializer from within the ./build/ folder
                    nob_temp_sprintf("-Wl,-rpath=./raylib/%s", MUSIALIZER_TARGET_NAME));
                nob_cmd_append(&cmd,
                    "-L./build",
                    "-l:raylib.dll");
                nob_cmd_append(&cmd, "-lwinmm", "-lgdi32");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        if (!nob_procs_wait(procs)) nob_return_defer(false);
#else
    cmd.count = 0;
        nob_cmd_append(&cmd, "clang");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
        #ifndef CONSOLE_MODE
        nob_cmd_append(&cmd, "-Wl,/SUBSYSTEM:WINDOWS,/ENTRY:mainCRTStartup");
        #endif
        nob_cmd_append(&cmd, "-I.");
        nob_cmd_append(&cmd, "-I"RAYLIB_SRC_FOLDER);
        nob_cmd_append(&cmd, "-o", "./build/musializer.exe");
        nob_cmd_append(&cmd,
            "./src/plug.c",
            "./src/ffmpeg_windows.c",
            "./src/musializer.c",
            "./thirdparty/tinyfiledialogs.c",
            "./build/musializer.res"
            );
        nob_cmd_append(&cmd,
            nob_temp_sprintf("-L./build/raylib/%s", MUSIALIZER_TARGET_NAME),
            "-lraylib");
        nob_cmd_append(&cmd, "-lwinmm", "-lgdi32", "-lole32", "-luser32", "-lshell32", "-lcomdlg32");
        nob_cmd_append(&cmd, "-static");
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
#endif // MUSIALIZER_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    return result;
}

bool build_raylib(void)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_File_Paths object_files = {0};

    if (!nob_mkdir_if_not_exists("./build/raylib")) {
        nob_return_defer(false);
    }

    Nob_Procs procs = {0};

    const char *build_path = nob_temp_sprintf("./build/raylib/%s", MUSIALIZER_TARGET_NAME);

    if (!nob_mkdir_if_not_exists(build_path)) {
        nob_return_defer(false);
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
        const char *input_path = nob_temp_sprintf(RAYLIB_SRC_FOLDER"%s.c", raylib_modules[i]);
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
        output_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);

        nob_da_append(&object_files, output_path);

        if (nob_needs_rebuild(output_path, &input_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP", "-DSUPPORT_FILEFORMAT_FLAC=1");
            nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
            nob_cmd_append(&cmd, "-I"RAYLIB_SRC_FOLDER"external/glfw/include");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);

            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }
    cmd.count = 0;

    if (!nob_procs_wait(procs)) nob_return_defer(false);

#ifndef MUSIALIZER_HOTRELOAD
    const char *libraylib_path = nob_temp_sprintf("%s/raylib.lib", build_path);

    if (nob_needs_rebuild(libraylib_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, "llvm-ar");
        nob_cmd_append(&cmd, "-crs", libraylib_path);
        for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
            nob_cmd_append(&cmd, input_path);
        }
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }
#else
    // it cannot load the raylib dll if it not in the same folder as the executable
    const char *libraylib_path = "./build/raylib.dll";

    if (nob_needs_rebuild(libraylib_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, "clang");
        nob_cmd_append(&cmd, "-shared");
        nob_cmd_append(&cmd, "-o", libraylib_path);
        for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
            nob_cmd_append(&cmd, input_path);
        }
        nob_cmd_append(&cmd, "-lwinmm", "-lgdi32");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }
#endif // MUSIALIZER_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(object_files);
    return result;
}

bool build_dist(void)
{
#ifdef MUSIALIZER_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!nob_mkdir_if_not_exists("./musializer-win64-clang/")) return false;
    if (!nob_copy_file("./build/musializer.exe", "./musializer-win64-clang/musializer.exe")) return false;
    if (!nob_copy_directory_recursively("./resources/", "./musializer-win64-clang/resources/")) return false;
    if (!nob_copy_file("musializer-logged.bat", "./musializer-win64-clang/musializer-logged.bat")) return false;
    // TODO: pack ffmpeg.exe with windows build
    //if (!nob_copy_file("ffmpeg.exe", "./musializer-win64-mingw/ffmpeg.exe")) return false;
    Nob_Cmd cmd = {0};
    const char *dist_path = "./musializer-win64-mingw.zip";
    nob_cmd_append(&cmd, "zip", "-r", dist_path, "./musializer-win64-mingw/");
    bool ok = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    if (!ok) return false;
    nob_log(NOB_INFO, "Created %s", dist_path);
    return true;
#endif // MUSIALIZER_HOTRELOAD
}


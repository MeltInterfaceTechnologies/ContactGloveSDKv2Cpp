# Local compatibility patch for pinned oscpack 3e7e3a102e316d75fe176885a6dd08f216f2eac0.
# Generate only the selected UdpSocket.cpp in the build tree. Never edit the
# FetchContent checkout (which may be shared by other projects). Its complete
# upstream copyright/license header is retained in the generated source.
function(contactglove_patch_oscpack_socket platform output_variable)
    set(input "${oscpack_SOURCE_DIR}/ip/${platform}/UdpSocket.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    file(READ "${input}" source)
    # Git may check out CRLF on Windows; verify normalized upstream content.
    string(REPLACE "\r\n" "\n" source "${source}")
    string(SHA256 actual_hash "${source}")
    if(platform STREQUAL "posix")
        set(expected_hash "8256da0a5ca9966aac1ede597522d749cd166f7e53781ee132931704171ef106")
    elseif(platform STREQUAL "win32")
        set(expected_hash "d6bc96df24b28936524976c517bdae94e582d759128c5bd9879bcb06d8832add")
    else()
        message(FATAL_ERROR "Unsupported oscpack socket platform: ${platform}")
    endif()
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR
            "oscpack ${platform}/UdpSocket.cpp differs from the pinned revision. "
            "Review cmake/PatchOscpack.cmake before updating or overriding oscpack.")
    endif()

    string(REPLACE "#include <algorithm>" "#include <algorithm>\n#include <atomic>" source "${source}")
    string(REPLACE "volatile bool break_;" [=[// Break/AsynchronousBreak may race the receiver thread. Volatile is
    // not synchronization; require lock-free atomics for signal-handler use.
    static_assert( ATOMIC_BOOL_LOCK_FREE == 2, "lock-free stop flag required" );
    std::atomic<bool> break_;]=] source "${source}")
    string(REPLACE "    Implementation()\n" "    Implementation()\n        : break_( false )\n" source "${source}")
    string(REPLACE "\t\tbreak_ = false;" [=[        // A stop may arrive before the receive thread enters Run. Consume it
        // on exit, not entry. Sequential reuse requires joining both this Run
        // and its stop caller first; concurrent Runs are unsupported.
        struct ResetStopOnExit {
            std::atomic<bool>& flag;
            ~ResetStopOnExit() { flag.store( false ); }
        } resetStopOnExit{ break_ };]=] source "${source}")

    if(platform STREQUAL "posix")
        # Early cancellation skips the receive loop, so consume pending wake
        # bytes on exit as well. Otherwise repeated early-stop/reuse can fill
        # the blocking pipe. Only Run reads this pipe; select-ready cannot be
        # stolen by another reader. Keep the destructor non-throwing.
        string(REPLACE "std::atomic<bool>& flag;" "std::atomic<bool>& flag;\n            int wakePipe;" source "${source}")
        string(REPLACE "~ResetStopOnExit() { flag.store( false ); }" [=[~ResetStopOnExit() {
                for( ;; ) {
                    fd_set readable;
                    FD_ZERO( &readable );
                    FD_SET( wakePipe, &readable );
                    timeval timeout = { 0, 0 };
                    int ready = select( wakePipe + 1, &readable, 0, 0, &timeout );
                    if( ready < 0 && errno == EINTR ) continue;
                    if( ready <= 0 ) break;
                    char pending[256];
                    ssize_t count = read( wakePipe, pending, sizeof(pending) );
                    if( count < 0 && errno == EINTR ) continue;
                    if( count <= 0 ) break;
                }
                flag.store( false );
            }]=] source "${source}")
        string(REPLACE "resetStopOnExit{ break_ };" "resetStopOnExit{ break_, breakPipe_[0] };" source "${source}")
    endif()

    set(output "${CMAKE_CURRENT_BINARY_DIR}/oscpack-patched/${platform}/UdpSocket.cpp")
    # configure_file(COPYONLY) avoids recompilation on unchanged reconfigure.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/oscpack-patched/${platform}")
    file(WRITE "${output}.tmp" "${source}")
    configure_file("${output}.tmp" "${output}" COPYONLY)
    file(REMOVE "${output}.tmp")
    set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

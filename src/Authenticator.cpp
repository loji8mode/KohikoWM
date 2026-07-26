#include "Authenticator.h"

#include <security/pam_appl.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace Kohiko
{

namespace
{

// The password conv() gets called with - pam_unix (and most other
// auth modules) only ever asks one PAM_PROMPT_ECHO_OFF question
// ("Password:"), so there's no real back-and-forth to model here,
// unlike an interactive terminal-based PAM client.
struct ConversationData
{
    const std::string* password;
};

// PAM's own conversation callback signature (see pam_conv(3)) - PAM
// calls this itself, once per pam_authenticate()/pam_acct_mgmt() call,
// to ask whatever its configured modules need asked. `appDataPtr` is
// whatever was set as pam_conv.appdata_ptr below.
int Conversation(
    int numMessages,
    const struct pam_message** messages,
    struct pam_response** responsesOut,
    void* appDataPtr)
{
    if (numMessages <= 0)
        return PAM_CONV_ERR;

    auto* responses = static_cast<struct pam_response*>(
        std::calloc(static_cast<std::size_t>(numMessages), sizeof(struct pam_response)));

    if (!responses)
        return PAM_BUF_ERR;

    const ConversationData* data = static_cast<const ConversationData*>(appDataPtr);

    for (int i = 0; i < numMessages; ++i)
    {
        switch (messages[i]->msg_style)
        {
            // The only two message types this ever actually answers -
            // both get the same password, which is correct for every
            // stock pam_unix "Password:" prompt. Anything else (an
            // informational PAM_TEXT_INFO/PAM_ERROR_MSG banner some
            // modules print) just gets left as a null response, which
            // PAM treats as "acknowledged, nothing to say back".
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                responses[i].resp = strdup(data->password->c_str());
                responses[i].resp_retcode = 0;
                break;

            default:
                responses[i].resp = nullptr;
                responses[i].resp_retcode = 0;
                break;
        }
    }

    *responsesOut = responses;
    return PAM_SUCCESS;
}

// Closes everything except stdin/stdout/stderr - called first thing
// in the forked child, before touching PAM at all. Without this, the
// child (and PAM's own further-forked unix_chkpwd helper underneath
// it) inherits every file descriptor the *parent* process happened to
// have open - Kohiko's X11 connection, its kohikoctl IPC socket, and
// whatever else - and this was reliably reproducible as a genuine
// hang: PAM's own internal pipe-based communication with unix_chkpwd
// waits on a select()/poll() that a stray inherited descriptor can
// leave sitting there forever, since the read side never sees an EOF
// it would otherwise see once every *actual* reference to the write
// end has closed. A minimal, standalone repro of just "fork(), then
// call PAM" never showed this - it only ever showed up with everything
// a real, fully-running window manager has open at once - which is
// exactly why forking a clean child is worth doing here rather than
// only in-process.
void CloseInheritedFileDescriptors()
{
    DIR* dir = opendir("/proc/self/fd");

    if (!dir)
        return;

    // Collected first, then closed - closedir() itself has an fd open
    // for the very directory being iterated, and closing arbitrary
    // fds mid-readdir() risks invalidating that iteration.
    std::vector<int> fds;

    while (struct dirent* entry = readdir(dir))
    {
        char* end = nullptr;
        long fd = std::strtol(entry->d_name, &end, 10);

        if (end != entry->d_name && *end == '\0' && fd > 2)
            fds.push_back(static_cast<int>(fd));
    }

    closedir(dir);

    for (int fd : fds)
        close(fd);
}

}

bool Authenticator::AuthenticateInChildProcess(
    const std::string& username,
    const std::string& password)
{
    ConversationData data{&password};
    struct pam_conv conv{&Conversation, &data};

    pam_handle_t* handle = nullptr;

    if (pam_start("kohiko", username.c_str(), &conv, &handle) != PAM_SUCCESS)
        return false;

    int result = pam_authenticate(handle, 0);

    if (result == PAM_SUCCESS)
        result = pam_acct_mgmt(handle, 0);

    pam_end(handle, result);

    return result == PAM_SUCCESS;
}

bool Authenticator::Authenticate(
    const std::string& username,
    const std::string& password)
{
    // See this class's own header comment for why this runs in a
    // forked child rather than directly here. SIGCHLD is temporarily
    // put back to its default disposition around the fork/wait:
    // Kohiko normally runs with it set to SIG_IGN (see
    // Application.cpp) so autostart-spawned processes never turn into
    // zombies, but that same setting makes waitpid() below unusable -
    // with SIGCHLD ignored, a terminated child can be reaped
    // automatically by the kernel before this process's own
    // waitpid() call ever collects its exit status, which is
    // indistinguishable from "no such child" (ECHILD).
    struct sigaction previousAction{};
    struct sigaction defaultAction{};
    defaultAction.sa_handler = SIG_DFL;
    sigemptyset(&defaultAction.sa_mask);

    sigaction(SIGCHLD, &defaultAction, &previousAction);

    pid_t pid = fork();

    if (pid < 0)
    {
        sigaction(SIGCHLD, &previousAction, nullptr);
        return false;
    }

    if (pid == 0)
    {
        // Child: nothing but the PAM conversation itself - no X11
        // connection, no other threads, none of whatever else this
        // process was doing a moment ago. Exit status is the only
        // thing that makes it back to the parent.
        CloseInheritedFileDescriptors();

        bool ok = AuthenticateInChildProcess(username, password);
        _exit(ok ? 0 : 1);
    }

    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);

    sigaction(SIGCHLD, &previousAction, nullptr);

    return waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}

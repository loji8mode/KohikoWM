#pragma once

#include <string>

namespace Kohiko
{

// Thin wrapper around libpam for LockScreen's password check - see
// LockScreen.cpp for the one place this gets called, and this
// project's `pam/kohiko` file, which needs to be installed as
// `/etc/pam.d/kohiko` (see the README's Building section) for
// well-defined behavior. Without it, PAM falls through to
// `/etc/pam.d/other`, whose contents vary by distro: Arch/Fedora/RHEL
// ship a strict deny-everything `other`, while Debian/Ubuntu's
// default `other` just includes the same common-auth stack a real
// account password would already pass - so a missing service file
// fails *closed* on some distros and fails *open* (silently
// authenticates against the ordinary login stack) on others. Install
// the file; don't rely on either fallback.
//
// Authenticate() runs the actual PAM conversation in a short-lived
// forked child rather than directly in this (fairly complex, several
// threads and X11 connections deep) process - pam_unix itself already
// forks its own small unix_chkpwd helper internally to check the
// password without needing raw read access to /etc/shadow, and in
// practice that inner fork() can interact badly with whatever else a
// large process happens to be doing at that exact moment (this was
// found - and reliably reproduced - while testing this class: with
// this process's SIGCHLD handling and threads left in place,
// pam_authenticate() could hang indefinitely; in a fresh, single-
// purpose child with nothing else going on, it works exactly as it
// should every time). Giving the whole PAM call the same kind of
// clean, minimal child process unix_chkpwd itself gets sidesteps the
// question entirely rather than needing to fully diagnose it.
class Authenticator
{
public:

    // Synchronous - blocks for as long as the fork/PAM conversation/
    // wait takes (typically near-instant for pam_unix). Returns true
    // only on a genuine successful authentication *and* a passing
    // account check (pam_acct_mgmt - catches an expired or locked
    // account even if the password itself was right).
    //
    // Also how LockScreen detects "this account has no password
    // configured" (see the spec's own wording for that case): calling
    // this with an empty `password` either succeeds (nothing further
    // to check) or fails as usual (a real password is required) -
    // there's no separate code path or /etc/shadow parsing needed for
    // that case at all.
    static bool Authenticate(
        const std::string& username,
        const std::string& password
    );

private:

    // The actual pam_start/pam_authenticate/pam_acct_mgmt/pam_end
    // sequence - always run inside the child process Authenticate()
    // forks, never directly in the caller's own process. See this
    // class's own header comment for why.
    static bool AuthenticateInChildProcess(
        const std::string& username,
        const std::string& password
    );

};

}

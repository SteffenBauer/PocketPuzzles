# Notice: History Rewritten on 2025-08-08

I previously included the compiled app binaries directly in the directory tree (previously in `release/` and very previously in `build/`).

As this turned out to be a bad idea, as it was bloating the repository size, this repository’s Git history was rewritten to remove these large binary files.

If you have an existing clone **please do this now before pulling the repository**:

* Delete your local clone and ***re-clone*** the whole repository again (*easiest* method).
* Or cleanup your local repository manually:
```
git fetch origin
git checkout master
git reset --hard origin/master
git reflog expire --expire-unreachable=now --all
git gc --prune=now --aggressive
```

If you don't do this, a new `git pull` with either fail or create a messy "frankenmerge".


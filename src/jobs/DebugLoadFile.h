#ifndef JOBS_DEBUGLOADFILE_H
#define JOBS_DEBUGLOADFILE_H

#include "../session.h"
#include "../Job.h"
#include "QueryResult.h"

// Tests loading a file that was previously saved using DebugSaveFile.
class DebugLoadFile
{
public:
    static QueryResult execute();
};

#endif  // JOBS_DEBUGLOADFILE_H

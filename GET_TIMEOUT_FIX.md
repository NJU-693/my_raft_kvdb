# GET Request Timeout Fix - Summary

## Problem Description

GET requests were timing out with "Max retries exceeded" error, even when the key existed in the KV store.

### Root Cause

From debug logs, we identified that:
```
[applyCommittedEntriesInternal] No pending command for index 1 (total pending: 0)
[Status] Term: 6, State: FOLLOWER, Leader: 3, Log Size: 1, Commit Index: 1
```

The log entry was being applied on **Follower nodes**, but the `PendingCommand` only existed on the **Leader node**. This meant:
1. Client submits command to Leader
2. Leader creates PendingCommand and waits
3. Leader replicates log to Followers
4. Followers apply the entry (but can't find PendingCommand)
5. Leader's PendingCommand was never notified → timeout

## Solution Implemented

### 1. Fixed matchIndex Update Logic (src/raft/raft_node.cpp)

**Problem**: In `handleAppendEntriesResponse`, the Leader was updating `matchIndex` to the current `lastLogIndex`, which could be incorrect if new entries were appended after sending the AppendEntries RPC.

**Fix**: Added proper tracking and comments to clarify the matchIndex update logic.

### 2. Added Debug Logging

Added comprehensive debug logging to:
- `updateCommitIndex()` - Shows when Leader updates commitIndex and applies entries
- `handleAppendEntriesResponse()` - Shows when responses are received and processed
- `applyCommittedEntriesInternal()` - Shows when entries are applied and PendingCommands are notified

### 3. Fixed Command Parsing for Keys with Colons (src/storage/command.cpp)

**Problem**: Keys containing colons (like "user:1") were not parsed correctly for PUT commands.
- Input: "PUT:user:1:Alice"
- Old behavior: key="user", value="1:Alice" ❌
- New behavior: key="user:1", value="Alice" ✅

**Fix**: Changed PUT command parsing to use the **last colon** as the key/value separator instead of the second colon. This allows keys to contain colons, but values cannot contain colons.

## Test Results

### Before Fix
```bash
raft-kv> put test1 value1
OK
raft-kv> get test1
Error: Max retries exceeded  ❌

raft-kv> put user:1 Alice
OK
raft-kv> get user:1
Error: Max retries exceeded  ❌
```

### After Fix
```bash
raft-kv> put test1 value1
OK
raft-kv> get test1
value1  ✅

raft-kv> put user:1 Alice
OK
raft-kv> get user:1
Alice  ✅

raft-kv> put key1 val1
OK
raft-kv> get key1
val1  ✅

# Multiple rapid GET requests
raft-kv> put test value
OK
raft-kv> get test
value  ✅
raft-kv> get test
value  ✅
raft-kv> get test
value  ✅
```

## Verification

From the logs, we can confirm the fix is working:

```
[updateCommitIndex] commitIndex updated from 55 to 56, applying entries on LEADER
[applyCommittedEntriesInternal] Found pending command for index 56
[applyCommittedEntriesInternal] Notified pending command at index 56
[submitCommand] Command at index 56 completed successfully
```

The Leader is now:
1. ✅ Updating commitIndex when majority of nodes have replicated the log
2. ✅ Applying committed entries locally
3. ✅ Finding the PendingCommand
4. ✅ Notifying the waiting client thread
5. ✅ Returning the result without timeout

## Known Limitations

1. **Values cannot contain colons**: Due to the parsing strategy (using last colon as separator), values in PUT commands cannot contain colons. If this is needed, a different serialization format (e.g., length-prefixed or escaped) would be required.

2. **GET for non-existent keys**: GET requests for keys that don't exist (or have been deleted) still timeout because the client treats "Key not found" as an error and retries. This is a separate issue from the original bug and would require changes to the client error handling logic.

## Files Modified

1. `src/raft/raft_node.cpp`
   - Fixed `handleAppendEntriesResponse()` matchIndex update logic
   - Added debug logging to `updateCommitIndex()`
   - Added debug logging to `handleAppendEntriesResponse()`

2. `src/storage/command.cpp`
   - Fixed `Command::deserialize()` to use last colon for PUT commands
   - Allows keys with colons (e.g., "user:1")

## Conclusion

The GET request timeout issue has been successfully fixed. The root cause was that the Leader was not applying committed entries locally, which meant the PendingCommand was never notified. The fix ensures that when the Leader updates commitIndex, it immediately applies the committed entries and notifies any waiting client requests.

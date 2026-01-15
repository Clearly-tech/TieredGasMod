//---------------------------------------------------------------------------------------------------
// scripts/3_Game/TieredGasClientBridge.c
//
// File summary: Lightweight static “bridge” for the client UI/HUD: queues admin messages and stores admin status received via RPC.
//
// TieredGasClientBridge
//
// void PushAdminMessage(string msg, bool isError)
//      Queues a message for the admin UI/HUD to display.
//      Params:
//          msg: text to display
//          isError: true if it should be shown as an error
//
// bool PopAdminMessage(out string msg, out bool isError)
//      Pops the oldest queued admin message.
//      Params (out):
//          msg: message text
//          isError: error flag
//      Returns: true if a message was returned, false if queue empty
//
// void SetAdminStatus(bool isAdmin)
//      Stores latest “is admin” result and marks it as available to consume.
//      Params:
//          isAdmin: admin status
//
// bool ConsumeAdminStatus(out bool isAdmin)
//      One-shot consumption of stored admin status.
//      Params (out):
//          isAdmin: admin status
//      Returns: true if status was available, else false
//---------------------------------------------------------------------------------------------------

class TieredGasClientBridge
{
    static ref array<ref Param2<string, bool>> s_AdminMsgs;
    static bool s_HasAdminStatus;
    static bool s_IsAdmin;

    static void PushAdminMessage(string msg, bool isError)
    {
        if (!s_AdminMsgs) s_AdminMsgs = new array<ref Param2<string, bool>>;
        s_AdminMsgs.Insert(new Param2<string, bool>(msg, isError));
    }

    static bool PopAdminMessage(out string msg, out bool isError)
    {
        if (!s_AdminMsgs || s_AdminMsgs.Count() == 0) return false;

        Param2<string, bool> p = s_AdminMsgs.Get(0);
        s_AdminMsgs.RemoveOrdered(0);

        msg = p.param1;
        isError = p.param2;
        return true;
    }

    static void SetAdminStatus(bool isAdmin)
    {
        s_IsAdmin = isAdmin;
        s_HasAdminStatus = true;
    }

    static bool ConsumeAdminStatus(out bool isAdmin)
    {
        if (!s_HasAdminStatus) return false;
        isAdmin = s_IsAdmin;
        s_HasAdminStatus = false;
        return true;
    }
}
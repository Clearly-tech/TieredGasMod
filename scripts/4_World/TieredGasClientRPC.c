//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasClientRPC.c
//
// File summary: Client-side RPC glue: receives admin status/messages and zone-related client updates.
//
// TieredGasClientRPC
//
// void HandleClientAdminRPC(ParamsReadContext ctx)
//      Reads admin RPC payload and forwards results to TieredGasClientBridge (messages/status).
//      Params:
//          ctx: RPC payload reader
//---------------------------------------------------------------------------------------------------
class TieredGasClientRPC
{
    static bool HandleClientAdminRPC(PlayerBase player, PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        if (GetGame().IsServer()) return false;

        if (rpc_type == RPC_ADMIN_MESSAGE)
        {
            Param2<string, bool> msgData;
            if (!ctx.Read(msgData)) return true;

            string message = msgData.param1;
            bool isError = msgData.param2;

            GetGame().GetMission().OnEvent(ChatMessageEventTypeID, new ChatMessageEventParams(CCDirect, "", message, ""));

            TieredGasClientBridge.PushAdminMessage(message, isError);
            return true;
        }

        if (rpc_type == RPC_ADMIN_CHECK_RESPONSE)
        {
            Param1<bool> adminStatus;
            if (!ctx.Read(adminStatus)) return true;

            TieredGasClientBridge.SetAdminStatus(adminStatus.param1);
            return true;
        }

        return false;
    }
}
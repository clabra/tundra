// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "WorldStream.h"
#include "RexLogicModule.h"

#include "RealXtend/RexProtocolMsgIDs.h"

#include "EC_OgrePlaceable.h"
#include "EC_OpenSimPrim.h"
#include "QuatUtils.h"
#include "ConversionUtils.h"
#include "SceneManager.h"

namespace ProtocolUtilities
{
    std::string WorldStream::LoggerName = "WorldStream";

    WorldStream::WorldStream(Foundation::Framework *framework) :
        framework_(framework),
        connected_(false),
        state_(ProtocolUtilities::Connection::STATE_DISCONNECTED),
        simName_(""),
        serverAddress_(""),
        serverPort_(0),
        connection_type_(DirectConnection)
    {
        clientParameters_.Reset();
	    SetCurrentProtocolType(ProtocolUtilities::NotSet);
    }

    WorldStream::~WorldStream()
    {

    }

    bool WorldStream::CreateUdpConnection()
    {
        bool connected_successfully;
	    protocolModule_ = GetCurrentProtocolModule();

        if (protocolModule_.get())
        {    
            clientParameters_ = protocolModule_->GetClientParameters();
            QUrl gridUrl(QString("http://%1").arg(clientParameters_.gridUrl.c_str()));

            if (gridUrl.isValid())
            {
                QString host = gridUrl.host();
                int port = gridUrl.port();

                if (!host.isEmpty() && !host.isNull() && port != -1)
                {
                    serverAddress_ = gridUrl.toString().toStdString();
                    connected_successfully = protocolModule_->CreateUdpConnection(host.toStdString().c_str(), port);
                }
                /*size_t pos = clientParameters_.gridUrl.rfind(":");
                std::string serveraddress_noport;
                if (pos == std::string::npos)
                    serveraddress_noport = clientParameters_.gridUrl;
                else
                {
                    serveraddress_noport = clientParameters_.gridUrl.substr(0, pos);
                    try
                    {
                        port = boost::lexical_cast<int>(clientParameters_.gridUrl.substr(pos + 1));
                    }
                    catch(std::exception)
                    {
                        LogError("Invalid port number, only numbers are allowed.");
                        return false;
                    }
                }*/

            }
            else
            {
                // What for is this here?? Can be removed? -Jonne
                connected_successfully = protocolModule_->CreateUdpConnection(serverAddress_.c_str(), serverPort_);
            }

            if (connected_successfully)
            {
                if (clientParameters_.agentID.IsNull() || clientParameters_.sessionID.IsNull())
                {
                    protocolModule_->DisconnectFromServer();
                    LogError("Client parameters agentID and sessionIDare not valid! Disconnecting...");
                    connected_ = false;
                    return false;
                }
                else
                {
                    connected_ = true;
                    SendLoginSuccessfullPackets();
                    LogInfo("Connected to server " + serverAddress_);
                    return true;
                }
            }
            else
                LogInfo("Connecting to server " + serverAddress_ + " failed");
        }
        else
            LogError("Getting current Protocol Module did not succeed");

        return false;
    }

    void WorldStream::SendLoginSuccessfullPackets()
    {
        // Send the necessary UDP packets.
        SendUseCircuitCodePacket();
        /// IS THE BELOW STATEMENT STILL RELEVANT?!
        ///
        //! \todo release mode viewer sends the following packets "too fast" for some old rexservers to cope. Wait a while.
        /*! Proper solution would be to wait for ack from the UseCircuitCode packet before continuing to send packets.
            It may also be that the issue is only an issue on a localhost server (ie. with no real network delay)
         */
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        SendCompleteAgentMovementPacket();
        SendAgentThrottlePacket();
        SendAgentWearablesRequestPacket();
        SendRexStartupPacket("started"); 
    }

    void WorldStream::RequestLogout()
    {
        if(!connected_)
            return;

        SendLogoutRequestPacket();
    }

    void WorldStream::ForceServerDisconnect()
    {
        if (!connected_)
            return;

	    protocolModule_ = GetCurrentProtocolModule();
	    if (protocolModule_.get())
	    {
		    connected_ = false;
            protocolModule_->DisconnectFromServer();
	    }
	    else
		    LogWarning("Could not force disconnect, can't get a handle to network interface");
    }

    void WorldStream::SendUseCircuitCodePacket()
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgUseCircuitCode);
        assert(m);

        m->AddU32(clientParameters_.circuitCode);
        m->AddUUID(clientParameters_.sessionID);
        m->AddUUID(clientParameters_.agentID);
        m->MarkReliable();

        FinishMessageBuilding(m);
    }

    void WorldStream::SendAgentWearablesRequestPacket()
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgAgentWearablesRequest);
        assert(m);
        
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->MarkReliable();

        FinishMessageBuilding(m);
    }

    void WorldStream::SendCompleteAgentMovementPacket()
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgCompleteAgentMovement);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddU32(clientParameters_.circuitCode);
        m->MarkReliable();

        FinishMessageBuilding(m);
    }

    void WorldStream::SendAgentThrottlePacket()
    {
        if(!connected_)
            return;

        Core::Real max_bits_per_second = framework_->GetDefaultConfig().DeclareSetting("RexLogicModule", "max_bits_per_second", 1000000.0f);

        int idx = 0;
        static const size_t size = 7 * sizeof(Core::Real);
        Core::u8 throttle_block[size];

        WriteFloatToBytes(max_bits_per_second * 0.1f, throttle_block, idx); // resend
        WriteFloatToBytes(max_bits_per_second * 0.1f, throttle_block, idx); // land
        WriteFloatToBytes(max_bits_per_second * 0.02f, throttle_block, idx); // wind
        WriteFloatToBytes(max_bits_per_second * 0.02f, throttle_block, idx); // cloud
        WriteFloatToBytes(max_bits_per_second * 0.25f, throttle_block, idx); // task
        WriteFloatToBytes(max_bits_per_second * 0.26f, throttle_block, idx); // texture
        WriteFloatToBytes(max_bits_per_second * 0.25f, throttle_block, idx); // asset

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgAgentThrottle);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddU32(clientParameters_.circuitCode);
        m->AddU32(0); // Generation counter
        m->AddBuffer(size, throttle_block); // throttles
        m->MarkReliable();

        FinishMessageBuilding(m);
    }

    void WorldStream::SendLogoutRequestPacket()
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgLogoutRequest);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        FinishMessageBuilding(m);

        LogInfo("Sent a Logout Request to the server... waiting for reply before quitting.");
    }

    void WorldStream::SendChatFromViewerPacket(const std::string &text)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgChatFromViewer);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBuffer(text.length(), (uint8_t*)text.c_str());
        m->AddU8(1);
        m->AddS32(0);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendImprovedInstantMessagePacket(const RexTypes::RexUUID &target, const std::string &text)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgImprovedInstantMessage);
        assert(m);

        unsigned int parent_estate_id = 0; //! @todo Find out proper value
        unsigned int time_stamp = 0; 
        std::string from_name = username_; //! @todo Find out proper value

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(false); // from group
        m->AddUUID(target);
        m->AddU32(parent_estate_id);
        m->AddUUID(clientParameters_.regionID); //! @todo Find out proper value
        m->AddVector3(RexTypes::Vector3());//! @todo Find out proper value
        m->AddU8(0);//! @todo Find out proper value
        m->AddU8(0); // dialog type
        m->AddUUID(RexTypes::RexUUID());
        m->AddU32(time_stamp); // TODO: Timestamp
        m->AddBuffer( strlen(from_name.c_str()), (uint8_t*)(from_name.c_str()) );
        m->AddBuffer( strlen(text.c_str()), (uint8_t*)(text.c_str()) );
        m->AddBuffer(0, NULL); // BinaryBucket

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectAddPacket(const RexTypes::Vector3 &position)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectAdd);
        assert(m);

        Vector3 scale(0.5f, 0.5f, 0.5f);
        Core::Quaternion rotation(0, 0, 0, 1);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddUUID(RexUUID());      // GroupID

        // ObjectData
        m->AddU8(0x09);             // PCode: 0x09 - prim
        m->AddU8(3);                // Material
        m->AddU32(0x02);            // AddFlags: 0x01 - use physics, 0x02 - create selected

        m->AddU8(16);               // PathCurve
        m->AddU8(1);                // ProfileCurve
        m->AddU16(0);               // PathBegin
        m->AddU16(0);               // PathEnd
        m->AddU8(100);              // PathScaleX
        m->AddU8(100);              // PathScaleY
        m->AddU8(0);                // PathShearX
        m->AddU8(0);                // PathShearY
        m->AddS8(0);                // PathTwist
        m->AddS8(0);                // PathTwistBegin
        m->AddS8(0);                // PathRadiusOffset
        m->AddS8(0);                // PathTaperX
        m->AddS8(0);                // PathTaperY
        m->AddU8(0);                // PathRevolutions
        m->AddS8(0);                // PathSkew
        m->AddU16(0);               // ProfileBegin
        m->AddU16(0);               // ProfileEnd
        m->AddU16(0);               // ProfileHollow
        m->AddU8(1);                // BypassRaycast
        m->AddVector3(position);    // RayStart     ///\note We use same position for both RayStart and RayEnd.
        m->AddVector3(position);    // RayEnd
        m->AddUUID(RexUUID());      // RayTargetID
        m->AddU8(0);                // RayEndIsIntersection
        m->AddVector3(scale);       // Scale
        m->AddQuaternion(rotation); // Rotation
        m->AddU8(0);                // State

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectDeletePacket(const uint32_t &local_id, const bool &force)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectDelete);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(force);

        // ObjectData
        m->SetVariableBlockCount(1);
        m->AddU32(local_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectDeletePacket(const std::vector<uint32_t> &local_id_list, const bool &force)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectDelete);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(force);

        // ObjectData
        m->SetVariableBlockCount(local_id_list.size());
        for(size_t i = 0; i < local_id_list.size(); ++i)
            m->AddU32(local_id_list[i]);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendAgentUpdatePacket(Core::Quaternion bodyrot, Core::Quaternion headrot, uint8_t state, 
        RexTypes::Vector3 camcenter, RexTypes::Vector3 camataxis, RexTypes::Vector3 camleftaxis, RexTypes::Vector3 camupaxis,
        float fardist, uint32_t controlflags, uint8_t flags)
    {
        if(!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgAgentUpdate);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddQuaternion(Core::OgreToOpenSimQuaternion(bodyrot));
        m->AddQuaternion(Core::OgreToOpenSimQuaternion(headrot));
        m->AddU8(state);
        m->AddVector3(camcenter);
        m->AddVector3(camataxis);
        m->AddVector3(camleftaxis);
        m->AddVector3(camupaxis);
        m->AddF32(fardist);
        m->AddU32(controlflags);
        m->AddU8(flags);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectSelectPacket(Core::entity_id_t object_id)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectSelect);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(1);
        m->AddU32(object_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectSelectPacket(std::vector<Core::entity_id_t> object_id_list)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectSelect);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(object_id_list.size());
        for(size_t i = 0; i < object_id_list.size(); ++i)
            m->AddU32(object_id_list[i]);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectDeselectPacket(Core::entity_id_t object_id)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectDeselect);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(1);
        m->AddU32(object_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectDeselectPacket(std::vector<Core::entity_id_t> object_id_list)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectDeselect);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(object_id_list.size());
        for(size_t i = 0; i < object_id_list.size(); ++i)
            m->AddU32(object_id_list[i]);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendMultipleObjectUpdatePacket(std::vector<Scene::EntityPtr> entity_ptr_list)
    {
        if (!connected_)
            return;

        // Pre-check that entities are valid, so that we don't start messagebuilding then abort
        for(size_t i = 0; i < entity_ptr_list.size(); ++i)
        {
            const Foundation::ComponentInterfacePtr &prim_component = entity_ptr_list[i]->GetComponent("EC_OpenSimPrim");
            if (!prim_component) 
            {
                /* the py api allows moving any entity with a placeable component, not just prims.
                   without this check an attempt to move the avatar from py crashed here */
                //LogWarning("Not sending entity position update of a non-prim entity (e.g. avatar), as the protocol doesn't support it.");
                return;
            }
        }
            
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgMultipleObjectUpdate);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        ///\todo Update just the necessary parameters (use update flags) & test with multiple objects.
        size_t offset = 0;
        uint8_t data[2048]; ///\todo What is the max size?

        m->SetVariableBlockCount(entity_ptr_list.size());

        for(size_t i = 0; i < entity_ptr_list.size(); ++i)
        {
            const Foundation::ComponentInterfacePtr &prim_component = entity_ptr_list[i]->GetComponent("EC_OpenSimPrim");

            RexLogic::EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(prim_component.get());

            const Foundation::ComponentInterfacePtr &ogre_component = entity_ptr_list[i]->GetComponent("EC_OgrePlaceable");
            OgreRenderer::EC_OgrePlaceable *ogre_pos = checked_static_cast<OgreRenderer::EC_OgrePlaceable *>(ogre_component.get());

            m->AddU32(prim->LocalId);
            m->AddU8(13);

            // Position
            memcpy(&data[offset], &Core::OgreToOpenSimCoordinateAxes(ogre_pos->GetPosition()), sizeof(Vector3));
            offset += sizeof(Vector3);

            // Scale
            memcpy(&data[offset], &Core::OgreToOpenSimCoordinateAxes(ogre_pos->GetScale()), sizeof(Vector3));
            offset += sizeof(Vector3);
        }

        // Add the data.
        m->AddBuffer(offset, data);

        ///\todo Make this work!
        /*for(size_t i = 0; i < entity_ptr_list.size(); ++i)
        {
            const Foundation::ComponentInterfacePtr &prim_component = entity_ptr_list[i]->GetComponent("EC_OpenSimPrim");
            RexLogic::EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(prim_component.get());

            const Foundation::ComponentInterfacePtr &ogre_component = entity_ptr_list[i]->GetComponent("EC_OgrePlaceable");
            OgreRenderer::EC_OgrePlaceable *ogre_pos = checked_static_cast<OgreRenderer::EC_OgrePlaceable *>(ogre_component.get());

            m->AddU32(prim->LocalId);
            m->AddU8(2);

            // Rotation
            Vector3 val = Core::PackQuaternionToFloat3(ogre_pos->GetOrientation());
            memcpy(&data[offset], &val, sizeof(Vector3));
            offset += sizeof(Vector3);
        }

        // Add the data.
        m->AddBuffer(offset, data);*/

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectNamePacket(std::vector<Scene::EntityPtr> entity_ptr_list)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectName);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(entity_ptr_list.size());
        for(size_t i = 0; i < entity_ptr_list.size(); ++i)
        {
            const Foundation::ComponentInterfacePtr &prim_component = entity_ptr_list[i]->GetComponent("EC_OpenSimPrim");
            RexLogic::EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(prim_component.get());
            
            m->AddU32(prim->LocalId);
            m->AddBuffer(prim->ObjectName.size(), (uint8_t*)prim->ObjectName.c_str());
        }
    }

    void WorldStream::SendObjectGrabPacket(Core::entity_id_t object_id)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectGrab);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->AddU32(object_id);
        //! \todo Touch offset is not send / calculated currently since it is not really used by the server anyway. -cm
        m->AddVector3(Vector3::ZERO);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendObjectDescriptionPacket(std::vector<Scene::EntityPtr> entity_ptr_list)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgObjectDescription);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // ObjectData
        m->SetVariableBlockCount(entity_ptr_list.size());
        for(size_t i = 0; i < entity_ptr_list.size(); ++i)
        {
            const Foundation::ComponentInterfacePtr &prim_component = entity_ptr_list[i]->GetComponent("EC_OpenSimPrim");
            RexLogic::EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(prim_component.get());
            
            m->AddU32(prim->LocalId);
            m->AddBuffer(prim->Description.size(), (uint8_t*)prim->Description.c_str());
        }
    }

    void WorldStream::SendRegionHandshakeReplyPacket(RexTypes::RexUUID agent_id, RexTypes::RexUUID session_id, uint32_t flags)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgRegionHandshakeReply);

        m->AddUUID(agent_id);
        m->AddUUID(session_id); 
        m->AddU32(flags);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendAgentSetAppearancePacket()
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgAgentSetAppearance);

        // Agentdata
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID); 
        m->AddU32(1);
        m->AddVector3(RexTypes::Vector3(0.45f, 0.6f, 1.708378f)); // Values for a default avatar height based on server

        // Wearabledata (empty)
        m->SetVariableBlockCount(0);
        m->AdvanceToNextVariable();

        // ObjectData (empty)
        m->AddBuffer(0,0);

        // VisualParam, these determine avatar's height on the server
        uint8_t visualparams[218];
        for(size_t i = 0; i < 218; ++i)
            visualparams[i] = 0;

        // Params based on source of server, which uses 218 params. 
        visualparams[25] = 122; // Body height
        visualparams[77] = 0; // Shoe heel height
        visualparams[78] = 0; // Shoe platform height
        visualparams[120] = 68; // Head size
        visualparams[125] = 178; // Leg length
        visualparams[148] = 117; // Neck length

        m->SetVariableBlockCount(218);
        for(size_t i = 0; i < 218; ++i)
            m->AddU8(visualparams[i]);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendCreateInventoryFolderPacket(
        const RexTypes::RexUUID &parent_id,
        const RexTypes::RexUUID &folder_id,
        const RexTypes::asset_type_t &type,
        const std::string &name)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgCreateInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // FolderData
        m->AddUUID(folder_id);
        m->AddUUID(parent_id);
        m->AddS8(type);
        ///\todo if "+1" doesn't exist, last char vanishes from the name, eg. "3D Models" -> "3D Model".
        m->AddBuffer(name.length() + 1, (uint8_t*)name.c_str());

        FinishMessageBuilding(m);
    }

    void WorldStream::SendMoveInventoryFolderPacket(
        const RexTypes::RexUUID &folder_id,
        const RexTypes::RexUUID &parent_id,
        const bool &re_timestamp)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgMoveInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(re_timestamp);

        // FolderData
        m->SetVariableBlockCount(1);
        m->AddUUID(folder_id);
        m->AddUUID(parent_id);

        FinishMessageBuilding(m);
    }

    ///\todo Test this function.
    /*
    void WorldStream::SendMoveInventoryFolderPacket(
        std::list<OpenSimProtocol::InventoryFolder *> folders,
        const bool &re_timestamp)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgMoveInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(re_timestamp);

        // FolderData, variable
        m->SetVariableBlockCount(folders.size());
        std::list<OpenSimProtocol::InventoryFolder *>::iterator it;
        for(it = folders.begin(); it != folders.end(); ++it)
        {
            OpenSimProtocol::InventoryFolder *folder = *it;
            m->AddUUID(folder->GetID());
            m->AddUUID(folder->GetParent()->GetID());
        }

        FinishMessageBuilding(m);
    }
    */

    void WorldStream::SendRemoveInventoryFolderPacket(
        const RexTypes::RexUUID &folder_id)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgRemoveInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // FolderData
        m->SetVariableBlockCount(1);
        m->AddUUID(folder_id);

        FinishMessageBuilding(m);
    }

    ///\todo Test this function.
    /*
    void WorldStream::SendRemoveInventoryFolderPacket(
        std::list<OpenSimProtocol::InventoryFolder *> folders)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgRemoveInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // FolderData, variable
        m->SetVariableBlockCount(folders.size());
        std::list<OpenSimProtocol::InventoryFolder *>::iterator it;
        for(it = folders.begin(); it != folders.end(); ++it)
        {
            OpenSimProtocol::InventoryFolder *folder = *it;
            m->AddUUID(folder->GetID());
        }

        FinishMessageBuilding(m);
    }
    */

    void WorldStream::SendMoveInventoryItemPacket(
        const RexTypes::RexUUID &item_id,
        const RexTypes::RexUUID &folder_id,
        const std::string &new_name,
        const bool &re_timestamp)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgMoveInventoryItem);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddBool(re_timestamp);

        // InventoryData, variable
        m->SetVariableBlockCount(1);
        m->AddUUID(item_id);
        m->AddUUID(folder_id);
        ///\todo if "+1" doesn't exist, last char vanishes from the name, eg. "3D Models" -> "3D Model".
        m->AddBuffer(new_name.length() + 1, (uint8_t*)new_name.c_str());

        FinishMessageBuilding(m);
    }

    void WorldStream::SendCopyInventoryItemPacket(
        const RexTypes::RexUUID &old_agent_id,
        const RexTypes::RexUUID &old_item_id,
        const RexTypes::RexUUID &new_folder_id,
        const std::string &new_name)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgCopyInventoryItem);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // InventoryData, variable
        m->SetVariableBlockCount(1);
        m->AddU32(0); // CallbackID, we don't need this.
        m->AddUUID(old_agent_id);
        m->AddUUID(old_item_id);
        m->AddUUID(new_folder_id);
        ///\todo if "+1" doesn't exist, last char vanishes from the name, eg. "3D Models" -> "3D Model".
        m->AddBuffer(new_name.length() + 1, (uint8_t*)new_name.c_str());

        FinishMessageBuilding(m);
    }

    void WorldStream::SendRemoveInventoryItemPacket(const RexTypes::RexUUID &item_id)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgRemoveInventoryItem);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // InventoryData, variable
        m->SetVariableBlockCount(1);
        m->AddUUID(item_id);

        FinishMessageBuilding(m);
    }

    ///\todo Test this function.
    void WorldStream::SendRemoveInventoryItemPacket(std::list<RexTypes::RexUUID> item_id_list)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgRemoveInventoryItem);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // InventoryData, variable
        m->SetVariableBlockCount(item_id_list.size());
        std::list<RexTypes::RexUUID>::iterator it;
        for(it = item_id_list.begin(); it != item_id_list.end(); ++it)
            m->AddUUID(*it);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendUpdateInventoryFolderPacket(
        const RexTypes::RexUUID &folder_id,
        const RexTypes::RexUUID &parent_id,
        const int8_t &type,
        const std::string &name)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgUpdateInventoryFolder);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // FolderData, variable
        m->SetVariableBlockCount(1);
        m->AddUUID(folder_id);
        m->AddUUID(parent_id);
        m->AddS8(type);
        ///\todo if "+1" doesn't exist, last char vanishes from the name, eg. "3D Models" -> "3D Model".
        m->AddBuffer(name.length() + 1, (uint8_t*)name.c_str());

        FinishMessageBuilding(m);
    }

    void WorldStream::SendUpdateInventoryItemPacket(
        const RexTypes::RexUUID item_id,
        const RexTypes::RexUUID folder_id,
        const RexTypes::asset_type_t &asset_type,
        const RexTypes::inventory_type_t &inventory_type,
        const std::string &name,
        const std::string &description)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgUpdateInventoryItem);
        assert(m);

        // TransactionID, new items only?
        RexTypes::RexUUID transaction_id;

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddUUID(transaction_id);

        // InventoryData, Variable
        m->SetVariableBlockCount(1);
        m->AddUUID(item_id);
        m->AddUUID(folder_id);
        m->AddU32(0);                       // CallbackID
        m->AddUUID(RexTypes::RexUUID());    // CreatorID
        m->AddUUID(RexTypes::RexUUID());    // OwnerID
        m->AddUUID(RexTypes::RexUUID());    // GroupID

        // Permissions-related:
        m->AddU32(0);                       //BaseMask
        m->AddU32(0);                       // OwnerMask
        m->AddU32(0);                       // GroupMask
        m->AddU32(0);                       // EveryoneMask
        m->AddU32(0);                       // NextOwnerMask
        m->AddBool(false);                  // GroupOwned

        m->AddUUID(transaction_id);
        m->AddS8(asset_type);               // Type
        m->AddS8(inventory_type);           // InvType
        m->AddU32(0);                       // Flags
        m->AddU8(0);                        // SaleType
        m->AddS32(0);                       // SalePrice
        m->AddBuffer(name.length() + 1, (uint8_t*)name.c_str());
        m->AddBuffer(description.length() + 1, (uint8_t*)description.c_str());
        m->AddS32(0);                       // CreationDAta
        m->AddU32(0);                       // CRC

        FinishMessageBuilding(m);
    }

    void WorldStream::SendFetchInventoryDescendentsPacket(
        const RexTypes::RexUUID &folder_id,
        const RexTypes::RexUUID &owner_id,
        const int32_t &sort_order,
        const bool &fetch_folders,
        const bool &fetch_items)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgFetchInventoryDescendents);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // InventoryData
        m->AddUUID(folder_id);
        if (owner_id.IsNull())
            m->AddUUID(clientParameters_.agentID);
        else
            m->AddUUID(owner_id);
        m->AddS32(sort_order);
        m->AddBool(fetch_folders);
        m->AddBool(fetch_items);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendAcceptFriendshipPacket(const RexTypes::RexUUID &transaction_id, const RexTypes::RexUUID &folder_id)
    {
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgAcceptFriendship);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddUUID(transaction_id);
        m->SetVariableBlockCount(1);
        m->AddUUID(folder_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendDeclineFriendshipPacket(const RexTypes::RexUUID &transaction_id)
    {
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgDeclineFriendship);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);
        m->AddUUID(transaction_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendFormFriendshipPacket(const RexTypes::RexUUID &dest_id)
    {
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgFormFriendship);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(dest_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendTerminateFriendshipPacket(const RexTypes::RexUUID &other_id)
    {
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgTerminateFriendship);
        assert(m);

        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(other_id);

        FinishMessageBuilding(m);
    }

    void WorldStream::SendGenericMessage(const std::string& method, const Core::StringVector& strings)
    {
        if (!connected_)
            return;

        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgGenericMessage);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // Transaction ID
        m->AddUUID(RexUUID::CreateRandom());

        // Method
        m->AddBuffer(method.length()+1, (uint8_t*)method.c_str());

        // Invoice ID
        m->AddUUID(RexUUID::CreateRandom());

        // Variable count of strings
        m->SetVariableBlockCount(strings.size());

        // Strings
        for(Core::uint i = 0; i < strings.size(); ++i)
            m->AddBuffer(strings[i].length()+1, (uint8_t*)strings[i].c_str());

        m->MarkReliable();
        
        FinishMessageBuilding(m);
    }


    void WorldStream::SendGenericMessageBinary(const std::string& method, const Core::StringVector& strings, const std::vector<uint8_t>& binary)
    {
        if (!connected_)
            return;

        const size_t max_string_size = 200;
        
        ProtocolUtilities::NetOutMessage *m = StartMessageBuilding(RexNetMsgGenericMessage);
        assert(m);

        // AgentData
        m->AddUUID(clientParameters_.agentID);
        m->AddUUID(clientParameters_.sessionID);

        // Transaction ID
        m->AddUUID(RexUUID::CreateRandom());

        // Method
        m->AddBuffer(method.length()+1, (uint8_t*)method.c_str());

        // Invoice ID
        m->AddUUID(RexUUID::CreateRandom());

        // See how many binary strings 
        size_t binarystrings = 0;
        size_t count = binary.size();
        while (count)
        {
            if (count > max_string_size)
                count -= max_string_size;
            else
                count -= count;
            binarystrings++;
        }
        
        // Variable count of strings
        m->SetVariableBlockCount(strings.size() + binarystrings);

        // Strings
        for(Core::uint i = 0; i < strings.size(); ++i)
            m->AddBuffer(strings[i].length()+1, (uint8_t*)strings[i].c_str());
        
        // Binary strings
        size_t idx = 0;
        count = binary.size();   
        for(Core::uint i = 0; i < binarystrings; ++i)
        {
            size_t size = count;
            if (size > max_string_size)
                size = max_string_size;
                
            m->AddBuffer(size, (uint8_t*)&binary[idx]);
            idx += size;
            count -= size;
        }

        m->MarkReliable();
        
        FinishMessageBuilding(m);
    }

    void WorldStream::SendRexStartupPacket(const std::string& state)
    {
        Core::StringVector strings;
        
        strings.push_back(clientParameters_.agentID.ToString());
        strings.push_back(state);
        SendGenericMessage("RexStartup", strings);
    }

    std::string WorldStream::GetCapability(const std::string &name)
    {
        protocolModule_ = GetCurrentProtocolModule();
        if (!protocolModule_.get())
        {
            LogError("Getting network interface did not succeed.");
            return "";
        }

        return protocolModule_->GetCapability(name);
    }

    volatile ProtocolUtilities::Connection::State WorldStream::GetConnectionState()
    {
	    protocolModule_ = GetCurrentProtocolModule();
	    if (!protocolModule_.get())
	    {
		    return ProtocolUtilities::Connection::STATE_ENUM_COUNT;
	    }
	    return protocolModule_->GetConnectionState();
    }

    ProtocolUtilities::NetOutMessage *WorldStream::StartMessageBuilding(const ProtocolUtilities::NetMsgID &message_id)
    {
        protocolModule_ = GetCurrentProtocolModule();
        if (!protocolModule_.get())
        {
            LogError("Getting network interface did not succeed.");
            return 0;
        }
        return protocolModule_->StartMessageBuilding(message_id);
    }

    void WorldStream::FinishMessageBuilding(ProtocolUtilities::NetOutMessage *msg)
    {
        protocolModule_ = GetCurrentProtocolModule();
        if (!protocolModule_.get())
        {
            LogError("Getting network interface did not succeed.");
            return;
        }
        protocolModule_->FinishMessageBuilding(msg);
    }

    const void WorldStream::SetCurrentProtocolType(ProtocolUtilities::ProtocolType newType)
    {
	    currentProtocolType_ = newType;
	    if ( currentProtocolType_ == ProtocolUtilities::OpenSim )
	    {
		    netInterfaceTaiga_ = boost::shared_ptr<TaigaProtocol::ProtocolModuleTaiga>();
		    netInterfaceOpenSim_ = framework_->GetModuleManager()->GetModule<OpenSimProtocol::ProtocolModuleOpenSim>(Foundation::Module::MT_OpenSimProtocol);
		    boost::shared_ptr<OpenSimProtocol::ProtocolModuleOpenSim> sp = netInterfaceOpenSim_.lock();
		    if ( !sp.get() )
		    {
			    LogError("Getting ProtocolModuleOpenSim network interface did not succeed");
		    }
	    }
	    else if (currentProtocolType_ == ProtocolUtilities::Taiga)
	    {
		    netInterfaceOpenSim_ = boost::shared_ptr<OpenSimProtocol::ProtocolModuleOpenSim>();
		    netInterfaceTaiga_ = framework_->GetModuleManager()->GetModule<TaigaProtocol::ProtocolModuleTaiga>(Foundation::Module::MT_TaigaProtocol);
		    boost::shared_ptr<TaigaProtocol::ProtocolModuleTaiga> sp = netInterfaceTaiga_.lock();
		    if ( !sp.get() )
		    {
			    LogError("Getting ProtocolModuleTaiga network interface did not succeed");
		    }
	    }
	    else if (currentProtocolType_ == ProtocolUtilities::NotSet)
	    {
		    LogError("Setting ProtocolType to NotSet");
	    }
    }

    boost::shared_ptr<ProtocolUtilities::ProtocolModuleInterface> WorldStream::GetCurrentProtocolModule()
    {
	    if ( currentProtocolType_ == ProtocolUtilities::OpenSim )
		    return netInterfaceOpenSim_.lock();
	    else if ( currentProtocolType_ == ProtocolUtilities::Taiga )
		    return netInterfaceTaiga_.lock();
	    else
		    return boost::shared_ptr<ProtocolUtilities::ProtocolModuleInterface>();
    }

    boost::weak_ptr<ProtocolUtilities::ProtocolModuleInterface> WorldStream::GetCurrentProtocolModuleWeakPointer()
    {
	    if ( currentProtocolType_ == ProtocolUtilities::OpenSim )
		    return netInterfaceOpenSim_;
	    else if ( currentProtocolType_ == ProtocolUtilities::Taiga )
		    return netInterfaceTaiga_;
	    else if ( currentProtocolType_ == ProtocolUtilities::NotSet )
		    return boost::weak_ptr<ProtocolUtilities::ProtocolModuleInterface>();
	    else
		    return boost::weak_ptr<ProtocolUtilities::ProtocolModuleInterface>();
    }

    const bool WorldStream::PrepareCurrentProtocolModule()
    {
	    protocolModule_ = GetCurrentProtocolModule();
	    if (protocolModule_.get())
	    {
		    protocolModule_->RegisterNetworkEvents();
		    return true;
	    }
	    else
	    {
		    LogWarning("Could not prepare current Protocol Modules for Registering Network Events");
		    return false;
	    }
    }

    const void WorldStream::UnregisterCurrentProtocolModule()
    {
	    protocolModule_ = GetCurrentProtocolModule();
	    if (protocolModule_.get())
		    protocolModule_->UnregisterNetworkEvents();
    }

    void WorldStream::WriteFloatToBytes(float value, uint8_t* bytes, int& idx)
    {
        *(float*)(&bytes[idx]) = value;
        idx += sizeof(float);
    }

} // namespace ProtocolUtilities

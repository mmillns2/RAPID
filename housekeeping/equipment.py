import midas
import midas.frontend
import midas.event

from dbreader import DBReader

class CryoPeriodicEquipment(midas.frontend.EquipmentBase):
    def __init__(self, client, dbreader):
        equip_name = "HouseKeeping"
        
        # Define the "common" settings of a frontend. These will appear in
        # /Equipment/MyPeriodicEquipment/Common. The values you set here are
        # only used the very first time this frontend/equipment runs; after 
        # that the ODB settings are used.
        default_common = midas.frontend.InitialEquipmentCommon()
        default_common.equip_type = midas.EQ_PERIODIC
        default_common.buffer_name = "SYSTEM"
        default_common.trigger_mask = 0
        default_common.event_id = 2
        default_common.period_ms = 3000
        default_common.read_when = midas.RO_RUNNING
        default_common.log_history = 0
        
        # MUST call midas.frontend.EquipmentBase.__init__ in your equipment's __init__ method!
        midas.frontend.EquipmentBase.__init__(self, client, equip_name, default_common)

        self.dbreader = dbreader
        
        # Can set the status of the equipment (appears in the midas status page)
        self.set_status("Initialized")
        
    def readout_func(self):
        '''
        For a periodic equipment, this function will be called periodically
        (every 100ms in this case). It should return either a `midas.event.Event`
        or None (if we shouldn't write an event).
        '''
        
        data = self.dbreader.read_latest()

        if data is None:
            return None

        event = midas.event.Event()

        # enforce stable ordering
        names = self.dbreader.channels
        values = [data["values"].get(ch, 0.0) for ch in names]

        # store DB timestamp in ms
        ts_ms = int(data["time"] * 1000)
        event.create_bank("TIME", midas.TID_QWORD, [ts_ms])
        
        # create one big bank to store data in
        event.create_bank("CRYO", midas.TID_FLOAT, values)

        return event

    def write_odb_mapping(self):

        names = self.dbreader.channels
        scids = [self.dbreader.scids[ch] for ch in names]

        path = f"/Equipment/{self.name}/Settings"

        self.client.odb_set(f"{path}/ChannelNames", names)
        self.client.odb_set(f"{path}/SCIDs", scids)

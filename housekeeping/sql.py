import psycopg2
import datetime
import time
import numpy as np

def dateFromTimeStamp(time,format):
    return datetime.datetime.fromtimestamp(int(time)).strftime(format)

class SQL:
    def __init__(self,debug,options):
        self.Debug = debug
        host = options[0]
        user = options[1]
        port = int(options[2])
        db = options[3]
        print("SQL: Connecting to postgres database = %s with username = %s, port = %d, host = %s" % (db,user,port,host))
        psqlConnect = "dbname=%s user=%s host=%s port=%d" % (db,user,host,port)
        try:
            self.db = psycopg2.connect(psqlConnect)
            self.schema = 'public.'
            self.DBconn = self.db.cursor()
        except:
            print("SQL error ...")


    def commit(self):
        self.db.commit()
        
    def executeSQL(self,sql_str):    
        if (self.Debug):
            print("SQL(): executeSQL: %s" % (sql_str))
        try:
            self.DBconn.execute(sql_str)
            self.db.commit()
        except:
            print("SQL(): executeSQL: error")
            self.db.rollback()

    def firstUpdate(self):
        sql_str = "select MIN(time) from %sslow_control_data" % (self.schema)
        self.DBconn.execute(sql_str)
        if (self.DBconn.rowcount != 1):
            print("ERROR: SQL(): lastUpdate() did not return exactly one row")
        else:
            row = self.DBconn.fetchone()[0]
            if (self.Debug):
                print("SQL(): lastUpdate() = %s" % (row))
            return row
        
    def lastUpdate(self):
        sql_str = "select MAX(time) from %sslow_control_data" % (self.schema)
        self.DBconn.execute(sql_str)
        if (self.DBconn.rowcount != 1):
            print("ERROR: SQL(): lastUpdate() did not return exactly one row")
        else:
            row = self.DBconn.fetchone()[0]
            if (self.Debug):
                print("SQL(): lastUpdate() = %s" % (row))
            return row

    def getSCID(self,name):
        sql = "select * from %sslow_control_items where name='%s'" % (self.schema,name)
        if (self.Debug):
            print("SQL(): getSCID: %s" % (sql))
        self.DBconn.execute(sql)
        if (self.DBconn.rowcount != 1):
            print("ERROR: SQL(): getSCID(%s) did not return exactly one row" % (name))
            return int(-1)
        else:
            row = self.DBconn.fetchone()[0]
            if (self.Debug):
                print("SQL(): getSCID(%s) = %d" % (name,row))
            return int(row)

    def insertSCValueByID(self, scid, value, timestamp):
        sql = "insert into slow_control_data (scid,value,time) values (%d,%f,'%s')" % (scid, value, timestamp)
        if (self.Debug):
            print("SQL(): insertSCValuebyID: %s" % (sql))
        try:
            self.DBconn.execute(sql)
            self.db.commit()
        except psycopg2.Error as e:
            print("Insert failed:", e)
            self.db.rollback()

    def insertSCValueByName(self,name,value,timestamp=None):

        if timestamp is None:
            timestamp = datetime.datetime.now()
        scid = self.getSCID(name)
        self.insertSCValueByID(scid, value, timestamp)
        

    def insertSCValuesByIDs(self,scids,values,timestamps=None):
        if timestamps is None:
            timestamps = [datetime.datetime.now() for _ in values]

        for scid, value, ts in zip(scids, values, timestamps):
            self.insertSCValueByID(scid, value, ts)

    def insertSCValuesByNames(self,names,values,timestamps=None):
        if timestamps is None:
            timestamps = [datetime.datetime.now() for _ in values]

        for name, value, ts in zip(names, values, timestamps):
            self.insertSCValueByName(name, value, ts)
            

    def getSCNames(self,scids):
        data = []
        for scid in scids:
            sql = "select name from %sslow_control_items where scid=%d" % (self.schema,scid)
            if (self.Debug):
                print("SQL(): getSCNames: %s" % (sql))
            self.DBconn.execute(sql)
            if (self.DBconn.rowcount != 1):
                print("ERROR: SQL(): getSCNames(%d) did not return exactly one row" % (scid))
                return int(-1)
            else:
                row = self.DBconn.fetchone()[0]
                data.append(row)        
                if (self.Debug):
                    print("SQL(): getSCNames(%d) = %s" % (scid,row))
                    
        return data

    def getSCTimes(self,start_time):
        data = []

        # ensure start_time is an integer
        if isinstance(start_time, datetime.datetime):
            start_time = int(start_time.timestamp())

        #sql = "select DISTINCT(time) from %sslow_control_data where time > %d" % (self.schema,start_time)
        sql = (
            "select DISTINCT(time) from %sslow_control_data "
            "where time > to_timestamp(%d)"
        ) % (self.schema, start_time)
        if (self.Debug):
            print("SQL(): getSCTimes: %s" % (sql))
        self.DBconn.execute(sql)
        if (self.DBconn.rowcount < 1):
            print("ERROR: SQL(): getSCTimes() returned no rows")
        else:    
            rows = self.DBconn.fetchall()
            for row in rows:
                time = int(row[0].timestamp())
                data.append(time)
        return data   

        
    def getSCValues(self,scids,start_time):
        # ensure start_time is an integer
        if isinstance(start_time, datetime.datetime):
            start_time = int(start_time.timestamp())

        num = len(scids)
        dicts = [dict() for i in range(num)]
        limit = 10000000

        data = []
            
        for scid in scids:
            #sql = "select scid,time,value from %sslow_control_data where scid=%d and time>= %d order by time limit %d" % (self.schema,scid,start_time,limit)
            sql = (
                "select scid,time,value from %sslow_control_data "
                "where scid=%d and time >= to_timestamp(%d) "
                "order by time limit %d"
            ) % (self.schema, scid, start_time, limit)
            if (self.Debug):
                print("SQL(): getSCValues: %s" % (sql))
            self.DBconn.execute(sql)
            if (self.DBconn.rowcount < 1):
                print("ERROR: SQL(): getSCValues(%s) returned no rows" % (scid))
            else:    
                rows = self.DBconn.fetchall()
                for row in rows:
#                    print row
                    scid  = row[0]
                    time = row[1]
                    value = row[2]
                    for i in range(num):
                        if (scid == scids[i]):
                            dicts[i][time] = value
#     Loop over times in 1st dict and then get values from subsequent dicts and create the structure and put in data[]

        times = sorted(dicts[0].keys())
        for time in times:
            d = {
                'time' : time }
            for i in range(num):
                value = dicts[i].get(time,-10)
                str = "value-%d" % (i+1)
                v = { str : value }
                d.update(v)
            data.append(d)
        return data
        
    def close(self):
        self.db.close()

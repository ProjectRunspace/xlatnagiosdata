# xlatnagiosdata

xlatnagiosdata includes tools to gather Nagios Core output, enter it into a database (currently only InfluxDB), and prepare it for display (Grafana expected).

It was originally designed as a replacement for [nagflux](https://github.com/Griesbacher/nagflux).

## Why xlatnagiosdata?

The basic need: Nagios Core gathers performance data, but it does not store it or provide performance graphing.

Other tools exist, all in various states. The most current and maintained tool set, nagflux and histou, did not meet our requirements. The creators of the xlatnagiosdata project nod to the creators and maintainers of nagflux and histou for their work. It provided the inspiration for this project.

## Data Captured by xlatnagiosdata

xlatnagiosdata captures these performance data points from Nagios Core:

* Timestamp of performance measurement
* Host name
* Short name of check command (if a host performance item) or service description (if a service performance item)
* All performance items reported by the check
    * Value
    * Unit of measurement
    * Warn level
    * Critical level
    * Minimum value
    * Maximum value

Nagios only requires that performance-reporting plugins provide a value. xlatnagiosdata will record whatever it receives.

## Completed Features

* daemon: xlatnagiosdatad
    * Captures performance data from Nagios
    * Translates data and units of measure from Nagios' standard to Grafana's standard (overridable and extensible)
    * Inserts translated data into an InfluxDB 1.x database named "nagiosrecords". If the database does not exist, creates it.
    * Preserves unusable data in a log file
    * Deletes files after successfully processing (either into InfluxDB or the log)

## Roadmap

1. Tool to convert nagflux's InfluxDB data to xlatnagiosdatad
2. Replace [histou](https://github.com/Griesbacher/histou). This tool creates links in Nagios for Grafana so that users do not need to create Grafana dashboards manually.
3. Update xlatnagiosdatad for InfluxDB security (https, authentication)
4. Update xlatnagiosdatad to work with InfluxDB 2.x.

The roadmap items must happen for our internal needs.

## Wish List

We have non-critical ideas that we want to implement.

* Automatic generation of complex, persistent Grafana dashboards
* Update xlatnagiosdatad to accept non-performance output from Nagios, such as acknowledgements. These can be overlaid on Grafana's performance history charts.
* Automatically read Nagios Core's configuration to handle some of the messy plumbing
* Implement a mechanism to process data files as they appear in the spool directory (this was abandoned during early development due to nasty and inexplicable problems with inotify, namely segfaults)

With our current project load, we won't promise any of these things. We welcome assistance!

## Prerequisites

We only tested on Ubuntu. We expect xlatnagiosdata to work on any Linux system that allows systemd services.

For this package to have any value, you must also have Nagios Core installed on the system where you run xlatnagiosdata. You must also have an InfluxDB 1.x installation somewhere. We only tested with InfluxDB installed on the Nagios system. However, xlatnagiosdata uses InfluxDB's http API, so changing the host name in the configuration file from "localhost" to the appropriate remote system, changing the port if necessary, and ensuring that you have connectivity on that port should work.

We do not (yet?) provide pre-built packages. You will need to compile the daemon to use it. You need these packages:
* make
* g++ (with C++ 20 support)
* curl libraries
* To follow our install directions, use git for cloning

On Ubuntu, you can install these with apt:

```
sudo apt install make build-essential git libcurl4-openssl-dev
```

Check your distribution's documentation for equivalents.

## Download and Compile

Download and compile the source:

```
cd ~
git clone https://github.com/ProjectRunspace/xlatnagiosdata.git
cd xlatnagiosdata
make all
```

If you run ```make``` by itself, it shows you several options.

## Automatic Installation

We provide an installer that:

* Copies the daemon to /usr/local/bin
* Creates /etc/xlatnagiosdata
* Copies an initial configuration file to /etc/xlatnagiosdata
* Copies a systemd service configuration file to /etc/systemd/system
* Enables the systemd service
* Reloads systemd daemons
* Starts the xlatnagiosdatad daemon
* Displays instructions for configuring Nagios

It makes several assumptions, especially regarding file locations. You can tweak everything later, so even if the daemon start fails, you can fix it more easily than performing a manual install.

From the directory created in the **Download and Compile section**, run:

```
sudo make install
```

**Note**: The automatic installer will NOT overwrite an existing configuration file. The automatic installer WILL overwrite the daemon (if not running) and service files.

At this point, the daemon should be running. Unless you installed over a previous installation, it's probably not getting any data to gather. Skip to the **Post-Installation Setup** section.

## Manual Installation

Perform these steps to manually install xlatnagiosdata:

1. Download and compile the daemon
2. Create a directory named ```/etc/xlatnagiosdata```
3. From the download directory, copy ```./daemon/config/xlatnagiosdatad.toml``` to ```/etc/xlatnagiosdata```
4. From the download directory, copy ```./daemon/config/xlatnagiosdatad.service``` to ```/etc/systemd/system``` (assuming that this is where your distribution places system service files)
5. From the download directory, copy ```xlatnagiosdatad``` to ```/usr/local/bin```. You may use an alternative location, but you must edit the service file from #4 to match.
6. If desired, edit the configuration file from step 3.
7. Reload system daemons: ```sudo systemctl daemon-reload```
8. Enable the xlatnagiosdata daemon: ```sudo systemctl enable xlatnagiosdatad``` (this sets the daemon to auto-start, you can save this step for later)
9. Start the xlatnagiosdata daemon: ```sudo systemctl start xlatnagiosdatad```

At this point, the daemon should be running. Unless you installed over a previous installation, it's probably not getting any data to gather. Skip to the **Post-Installation Setup** section.

## Starting, Stopping, Reloading, and Verifying the Daemon

The xlatnagiosdata daemon works like most systemd daemons.

Start the daemon with either of the two following commands:

```
sudo systemctl start xlatnagiosdatad
sudo service nagiosdatad start

```

Stop the daemon:
```
sudo systemctl stop xlatnagiosdatad
sudo service nagiosdatad stop

```

Reload the daemon without stopping it (re-reads the configuration file for changes):
```
sudo systemctl reload xlatnagiosdatad

```

## Post-Installation Setup

After install, you need to do a few things:

1. Create a location for Nagios to write performance data and xlatnagiosdata to read it
2. Enable Nagios to write performance data
3. Record where Nagios records performance data
4. Tell Nagios what format to use for performance data
5. Configure Nagios to process performance data
6. Create a command for processing performance data
7. Configure host and service checks to generate performance data
8. Restart Nagios to apply changes

### 1. Create a Location for Nagios to Write Performance Data

Nagios Core as installed from the Nagios web page installs to ```/usr/local/nagios```. Other repositories and repackaged solutions might use other locations. Under its root install location, Nagios creates a ```/var/spool``` sub-directory. Create an ```xlatnagiosdata``` directory. Example:

```
sudo mkdir /usr/local/nagios/xlatnagiosdata
```

**Note**: If you use a different location than ```/usr/local/nagios/xlatnagiosdata```, make sure to edit ```/etc/xlatnagiosdata/xlatnagiosdatad.toml``` to change the "spool_directory" key accordingly.

### 2. Enable Nagios to Write Performance Data

In the primary ```nagios.cfg``` file (usually in ```/usr/local/nagios/etc/```) add this line (we recommend commenting out the original, not deleting it):

```
process_performance_data=1
```

### 3. Record Where Nagios Records Performance Data

In the primary ```nagios.cfg``` file (usually in ```/usr/local/nagios/etc/```) find these lines and uncomment them:

```
host_perfdata_file
service_perfdata_file

```
Record their values (typically ```/usr/local/nagios/var/host-perfdata``` and ```/usr/local/nagios/var/service-perfdata```, respectively.)

### 4. Tell Nagios What Format to Use for Performance Data

In the primary ```nagios.cfg``` file (usually in ```/usr/local/nagios/etc/```) add these lines (we recommend commenting out the originals, not deleting them):

```
host_perfdata_file_template=$TIMET$\t$HOSTNAME$\t$HOSTCHECKCOMMAND$\t$HOSTPERFDATA$
service_perfdata_file_template=$TIMET$\t$HOSTNAME$\t$SERVICEDESC$\t$SERVICEPERFDATA$
```

### 5. Configure Nagios to Process Performance Data

In the primary ```nagios.cfg``` file (usually in ```/usr/local/nagios/etc/```) add these lines (we recommend commenting out the originals, not deleting them):

```
host_perfdata_file_processing_command=process-host-perfdata-xlatnagiosdata
service_perfdata_file_processing_command=process-service-perfdata-xlatnagiosdata
```

You can choose different names, but you must remember them for step 6.

### Step 6. Create a Command for Processing Performance Data

In any .cfg file of your choosing, typically in the ```objects``` directory underneath ```/usr/local/nagios/etc/```, create a command with the same name that you specified in step 5. Example:

```
define command {
   command_name process-host-perfdata-xlatnagiosdata
   command_line mv /usr/local/nagios/var/host-perfdata /var/spool/xlatnagiosdata/$TIMET$.perfdata.host
}

define command {
   command_name process-service-perfdata-xlatnagiosdata
   command_line mv /usr/local/nagios/var/service-perfdata /var/spool/xlatnagiosdata/$TIMET$.perfdata.service
}
```

The ```command_name```s must match what you used in step 5. The first parts after ```mv``` in the ```command_line``` must match the locations that you recorded in step 3. The final section must include the path from step 1. The examples used ```$TIMET$.perfdata.host``` and ```$TIMET$.perfdata.service``` as they will prevent collisions. xlatnagiosdata does not care what names the files have.

If you created a new .cfg file for this, then you must ensure that ```nagios.cfg``` enables it via ```cfg_file``` or ```cfg_dir```.

### Step 7. Configure Host and Service Checks to Generate Performance Data

```host``` and ```service``` objects in Nagios will only generate performance data if you set their ```process_perf_data``` key to ```1```. Example:

```
define host {
   name                 perf-host
	process_perf_data    1
	.
	.
	.
}
```

### Step 8. Check and Restart Nagios

Check nagios:
```
/usr/local/nagios/bin/nagios -v /usr/local/nagios/etc/nagios.cfg
```

Check for warnings and problems. Fix before proceeding.

Restart nagios:
```
sudo service nagios restart
```

## View xlatnagiosdata in InfluxDB

xlatnagiosdata automatically creates a "nagiosrecords" databases. Inside that database, it creates a measurement called "perfdata".

This document only shows a quick way to view and verify the data. For more in-depth InfluxDB operations, consult the [InfluxDB documentation](https://docs.influxdata.com/influxdb/v1).

Start the InfluxDB CLI:

```
influx
```

List the databases:

```
show databases
```

If you do not see an item named "nagiosrecords", then check the logs to see if xlatnagiosdata is working.

Switch to the xlatnagiosdata database:

```
use nagiosrecords
```

Show the "perfdata" measurement:

```
show measurements
```

View top-level items:

```
show series
```

Retrieve 50 data items:

```
select * from perfdata limit 50
```

Exit the InfluxDB CLI:

```
exit
```

You can check the data at any time to ensure updates.

## View xlatnagiosdata Logs

xlatnagiosdata attempts to log to files in ```/var/log/xlatnagiosdata```. Because the daemon runs as root by default, this should always work. In the event that it cannot write to that location, it writes to syslog.

View the operations log:

```
sudo cat /var/log/xlatnagiosdata/daemon.log
```

View the failed writes log:

```
sudo cat /var/log/xlatnagiosdata/failed_writes.log
```

## Remove xlatnagiosdata

The makefile includes two assistants for removal.

Uninstall:
```
sudo make uninstall
```

The ```uninstall``` directive stops and removes the daemon. It leaves everything else intact.

Purge:
```
sudo make purge
```

The ```purge``` directive stops and removes the daemon, then deletes the logs. It also deletes the configuration directory ```/etc/xlatnagiosdata```.

No automation exists to remove the spool directory or InfluxDB database. Instructions appear after manual service removal.

Manually remove service:

```
sudo systemctl stop xlatnagiosdatad
sudo systemctl disable xlatnagiosdatad
sudo rm -f /etc/systemd/system/xlatnagiosdatad.service
sudo systemctl daemon-reload
```

Manually remove logs and spool directory:

```
sudo rm -rf /var/log/xlatnagiosdata
```

Delete spool directory:

```
sudo rm -rf /usr/local/nagios/var/spool/xlatnagiosdata
```

Delete InfluxDB data:

```
influx
drop database nagiosrecords
exit
```

Manually remove configuration directory:

```
sudo rm -rf /etc/xlatnagiosdata
```

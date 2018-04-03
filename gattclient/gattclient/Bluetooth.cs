using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Collections.ObjectModel;
using System.Diagnostics;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using System.Threading;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;
using System.Runtime.InteropServices;

namespace gattclient
{
    
    [StructLayout(LayoutKind.Sequential, Pack=1)]
    struct OTABlob
    {
        public uint magic;
        public ushort total_size;
        public byte cur_chunk;
        public byte num_chunks;
        public ushort checksum;
        public ushort chunk_len;
        /* uint8_t blob[0]; */

        public byte[] ToBytes()
        {
            Byte[] bytes = new Byte[Marshal.SizeOf(typeof(OTABlob))];
            GCHandle pinStructure = GCHandle.Alloc(this, GCHandleType.Pinned);
            try
            {
                Marshal.Copy(pinStructure.AddrOfPinnedObject(), bytes, 0, bytes.Length);
                return bytes;
            }
            finally
            {
                pinStructure.Free();
            }
        }
    };

    class Bluetooth
    {
        private ObservableCollection<BluetoothLEDeviceDisplay> KnownDevices = new ObservableCollection<BluetoothLEDeviceDisplay>();
        private List<DeviceInformation> UnknownDevices = new List<DeviceInformation>();
        private DeviceWatcher deviceWatcher;
        private ManualResetEvent devicesEnumDone = new ManualResetEvent(false);
        private int delayAfterSend;

        private const int READ_CHARACTERISTIC_INDEX = 2;
        private const int WRITE_CHARACTERISTIC_INDEX = 2;

        private const int MAX_PAIR_ATTEMPTS = 10;
        private const int MAX_CONNECT_ATTEMPTS = 10;
        private const int MAX_SEND_ATTEMPTS = 10;

        public Bluetooth(int delayAfterSend)
        {
            this.delayAfterSend = delayAfterSend;
        }

        public bool Start(byte[] data)
        {
            Task<bool> task;

            EnumerateDevices();
            //devicesEnumDone.WaitOne();
            Console.WriteLine("Sleeping for 3 seconds...");
            Thread.Sleep(10 * 1000);

            //KnownDevices[0].DeviceInformation.Properties.Keys


            Debug.Assert(KnownDevices.Count >= 1);

            bool paired = false;
            for (int i = 0; i < MAX_PAIR_ATTEMPTS && !paired; i++)
            {
                Console.WriteLine($"[{i + 1}/{MAX_CONNECT_ATTEMPTS}] pairing...");
                task = Pair();
                task.Wait();
                paired = task.Result;
                if (!paired)
                {
                    Thread.Sleep(1000);
                }
            }

            if (!paired)
            {
                return false;
            }

            bool connected = false;
            for (int i = 0; i < MAX_CONNECT_ATTEMPTS && !connected; i++)
            {
                Console.WriteLine($"[{i + 1}/{MAX_CONNECT_ATTEMPTS}] connecting...");
                task = Connect();
                task.Wait();
                connected = task.Result;
                if (!connected)
                {
                    Thread.Sleep(1000);
                }
            }

            if (!connected)
            {
                return false;
            }

            bool sent = false;
            for (int i = 0; i < MAX_SEND_ATTEMPTS && !sent; i++)
            {
                Console.WriteLine($"[{i + 1}/{MAX_SEND_ATTEMPTS}] connecting...");
                task = SendByte(data);
                task.Wait();
                sent = task.Result;
                if (!sent)
                {
                    Thread.Sleep(1000);
                }
            }

            return sent;
            //byte[] res = task.Result;
            //Debug.Assert(data.SequenceEqual(res), $"sent smth but read smth'");
        }

        public bool Start(string text)
        {
            bool paired = false;
            EnumerateDevices();
            //devicesEnumDone.WaitOne();
            Thread.Sleep(3 * 1000);
            Debug.Assert(KnownDevices.Count == 1);
            Pair().Wait();
            Connect().Wait();
            SendByte(text).Wait();
            var task = ReadString();
            task.Wait();
            string res = task.Result;
            //Debug.Assert(res == text, $"sent {text} but read {res}");
            return false;
        }

        #region Pairing
        private async Task<bool> Pair()
        {
            Console.WriteLine("Pairing started. Please wait...");

            // For more information about device pairing, including examples of
            // customizing the pairing process, see the DeviceEnumerationAndPairing sample.

            // Capture the current selected item in case the user changes it while we are pairing.
            Debug.Assert(KnownDevices.Count == 1, "We shouldn't have more than one known device.");
            var bleDeviceDisplay = KnownDevices[0];

            // BT_Code: Pair the currently selected device.
            DevicePairingResult result = await bleDeviceDisplay.DeviceInformation.Pairing.PairAsync();
            //Debug.Assert(result.Status == DevicePairingResultStatus.AlreadyPaired ||
            //             result.Status == DevicePairingResultStatus.Paired);
            Console.WriteLine($"Pairing result = {result.Status}");
            return result.Status == DevicePairingResultStatus.AlreadyPaired ||
                   result.Status == DevicePairingResultStatus.Paired;
        }
        #endregion

        #region Device discovery
        private void EnumerateDevices()
        {
            if (deviceWatcher == null)
            {
                StartBleDeviceWatcher();
                Console.WriteLine($"Device watcher started.");
            }
            else
            {
                StopBleDeviceWatcher();
                Console.WriteLine($"Device watcher stopped.");
            }
        }

        /// <summary>
        /// Starts a device watcher that looks for all nearby Bluetooth devices (paired or unpaired). 
        /// Attaches event handlers to populate the device collection.
        /// </summary>
        private void StartBleDeviceWatcher()
        {
            // Additional properties we would like about the device.
            // Property strings are documented here https://msdn.microsoft.com/en-us/library/windows/desktop/ff521659(v=vs.85).aspx
            string[] requestedProperties = { "System.Devices.Aep.DeviceAddress", "System.Devices.Aep.IsConnected", "System.Devices.Aep.Bluetooth.Le.IsConnectable" };

            //string[] requestedProperties = { "System.Devices.Aep.DeviceAddress", "System.Devices.Aep.IsConnected" };

            // BT_Code: Example showing paired and non-paired in a single query.
            string aqsAllBluetoothLEDevices = "(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";

            //deviceWatcher =
            //        DeviceInformation.CreateWatcher(
            //            aqsAllBluetoothLEDevices,
            //            requestedProperties,
            //            DeviceInformationKind.AssociationEndpoint);

            deviceWatcher =
                    DeviceInformation.CreateWatcher(aqsAllBluetoothLEDevices, requestedProperties, DeviceInformationKind.AssociationEndpoint);
                        //aqsAllBluetoothLEDevices,
                        //requestedProperties,
                        //DeviceInformationKind.AssociationEndpoint);


            // Register event handlers before starting the watcher.
            deviceWatcher.Added += DeviceWatcher_Added;
            deviceWatcher.Updated += DeviceWatcher_Updated;
            deviceWatcher.Removed += DeviceWatcher_Removed;
            deviceWatcher.EnumerationCompleted += DeviceWatcher_EnumerationCompleted;
            deviceWatcher.Stopped += DeviceWatcher_Stopped;

            // Start over with an empty collection.
            KnownDevices.Clear();

            // Start the watcher.
            deviceWatcher.Start();
        }

        /// <summary>
        /// Stops watching for all nearby Bluetooth devices.
        /// </summary>
        private void StopBleDeviceWatcher()
        {
            if (deviceWatcher != null)
            {
                // Unregister the event handlers.
                deviceWatcher.Added -= DeviceWatcher_Added;
                deviceWatcher.Updated -= DeviceWatcher_Updated;
                deviceWatcher.Removed -= DeviceWatcher_Removed;
                deviceWatcher.EnumerationCompleted -= DeviceWatcher_EnumerationCompleted;
                deviceWatcher.Stopped -= DeviceWatcher_Stopped;

                // Stop the watcher.
                deviceWatcher.Stop();
                deviceWatcher = null;
            }
        }

        private BluetoothLEDeviceDisplay FindBluetoothLEDeviceDisplay(string id)
        {
            foreach (BluetoothLEDeviceDisplay bleDeviceDisplay in KnownDevices)
            {
                if (bleDeviceDisplay.Id == id)
                {
                    return bleDeviceDisplay;
                }
            }
            return null;
        }

        private DeviceInformation FindUnknownDevices(string id)
        {
            foreach (DeviceInformation bleDeviceInfo in UnknownDevices)
            {
                if (bleDeviceInfo.Id == id)
                {
                    return bleDeviceInfo;
                }
            }
            return null;
        }

        private void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo)
        {
            // We must update the collection on the UI thread because the collection is databound to a UI element.
            lock (this)
            {
                Console.WriteLine(string.Format("Added {0}{1}", deviceInfo.Id, deviceInfo.Name));
                Debug.WriteLine(String.Format("Added {0}{1}", deviceInfo.Id, deviceInfo.Name));

                // Protect against race condition if the task runs after the app stopped the deviceWatcher.
                if (sender == deviceWatcher)
                {
                    // Make sure device isn't already present in the list.
                    if (FindBluetoothLEDeviceDisplay(deviceInfo.Id) == null)
                    {
                        if (deviceInfo.Name != string.Empty)
                        {
                            // If device has a friendly name display it immediately.
                            KnownDevices.Add(new BluetoothLEDeviceDisplay(deviceInfo));
                        }
                        else
                        {
                            // Add it to a list in case the name gets updated later. 
                            UnknownDevices.Add(deviceInfo);
                        }
                    }

                }
            }
        }

        private void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
        {
            // We must update the collection on the UI thread because the collection is databound to a UI element.
            lock (this)
            {
                Console.WriteLine(string.Format("Updated {0}{1}", deviceInfoUpdate.Id, ""));
                Debug.WriteLine(String.Format("Updated {0}{1}", deviceInfoUpdate.Id, ""));

                // Protect against race condition if the task runs after the app stopped the deviceWatcher.
                if (sender == deviceWatcher)
                {
                    BluetoothLEDeviceDisplay bleDeviceDisplay = FindBluetoothLEDeviceDisplay(deviceInfoUpdate.Id);
                    if (bleDeviceDisplay != null)
                    {
                        // Device is already being displayed - update UX.
                        bleDeviceDisplay.Update(deviceInfoUpdate);
                        return;
                    }

                    DeviceInformation deviceInfo = FindUnknownDevices(deviceInfoUpdate.Id);
                    if (deviceInfo != null)
                    {
                        deviceInfo.Update(deviceInfoUpdate);
                        // If device has been updated with a friendly name it's no longer unknown.
                        if (deviceInfo.Name != String.Empty)
                        {
                            KnownDevices.Add(new BluetoothLEDeviceDisplay(deviceInfo));
                            UnknownDevices.Remove(deviceInfo);
                        }
                    }
                }
            }
        }

        private void DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
        {
            lock (this)
            {
                Console.WriteLine(string.Format("Removed {0}{1}", deviceInfoUpdate.Id, ""));
                Debug.WriteLine(String.Format("Removed {0}{1}", deviceInfoUpdate.Id, ""));

                // Protect against race condition if the task runs after the app stopped the deviceWatcher.
                if (sender == deviceWatcher)
                {
                    // Find the corresponding DeviceInformation in the collection and remove it.
                    BluetoothLEDeviceDisplay bleDeviceDisplay = FindBluetoothLEDeviceDisplay(deviceInfoUpdate.Id);
                    if (bleDeviceDisplay != null)
                    {
                        KnownDevices.Remove(bleDeviceDisplay);
                    }

                    DeviceInformation deviceInfo = FindUnknownDevices(deviceInfoUpdate.Id);
                    if (deviceInfo != null)
                    {
                        UnknownDevices.Remove(deviceInfo);
                    }
                }
            }
        }

        private void DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, object e)
        {
            // Protect against race condition if the task runs after the app stopped the deviceWatcher.
            if (sender == deviceWatcher)
            {
                Console.WriteLine($"{KnownDevices.Count} devices found. Enumeration completed.");
            }
            devicesEnumDone.Set();
        }

        private void DeviceWatcher_Stopped(DeviceWatcher sender, object e)
        {
            // Protect against race condition if the task runs after the app stopped the deviceWatcher.
            if (sender == deviceWatcher)
            {
                Console.WriteLine($"No longer watching for devices.");
            }
        }
        #endregion

        #region Connect
        private bool InConnectProgress;
        private ObservableCollection<BluetoothLEAttributeDisplay> CharacteristicCollection = new ObservableCollection<BluetoothLEAttributeDisplay>();

        // TODO: how do we fill this member?
        private BluetoothLEDevice bluetoothLeDevice = null;

        private ObservableCollection<BluetoothLEAttributeDisplay> ServiceCollection = new ObservableCollection<BluetoothLEAttributeDisplay>();
        private GattCharacteristic selectedCharacteristic;
        private GattPresentationFormat presentationFormat;

        private bool ClearBluetoothLEDeviceAsync()
        {
            bluetoothLeDevice?.Dispose();
            bluetoothLeDevice = null;
            return true;
        }


        private async Task<bool> ConnectImpl()
        {
            if (!ClearBluetoothLEDeviceAsync())
            {
                Console.WriteLine("Error: Unable to reset state, try again.");
                //Debug.Assert(false, "Error: Unable to reset state, try again.");
                return false;
            }

            try
            {
                var device = KnownDevices[0];
                // BT_Code: BluetoothLEDevice.FromIdAsync must be called from a UI thread because it may prompt for consent.
                bluetoothLeDevice = await BluetoothLEDevice.FromIdAsync(device.Id);

                if (bluetoothLeDevice == null)
                {
                    Console.WriteLine("Failed to connect to device.");
                    return false;
                }
            }
            catch (Exception ex) when (ex.HResult == E_DEVICE_NOT_AVAILABLE)
            {
                Console.WriteLine("Bluetooth radio is not on.");
            }

            if (bluetoothLeDevice != null)
            {
                // Note: BluetoothLEDevice.GattServices property will return an empty list for unpaired devices. For all uses we recommend using the GetGattServicesAsync method.
                // BT_Code: GetGattServicesAsync returns a list of all the supported services of the device (even if it's not paired to the system).
                // If the services supported by the device are expected to change during BT usage, subscribe to the GattServicesChanged event.
                GattDeviceServicesResult result = await bluetoothLeDevice.GetGattServicesAsync(BluetoothCacheMode.Uncached);

                if (result.Status == GattCommunicationStatus.Success)
                {
                    var services = result.Services;
                    Console.WriteLine(String.Format("Found {0} services", services.Count));
                    foreach (var service in services)
                    {
                        ServiceCollection.Add(new BluetoothLEAttributeDisplay(service));
                    }

                    return true;
                }
                else
                {
                    Console.WriteLine("Device unreachable");
                    //Debug.Assert(false, "Device unreachable");
                    return false;
                }
            }

            return false;
        }

        private async Task<bool> Connect()
        {
            bool res = await ConnectImpl();
            if (!res)
                return res;

            return await EnumerateCharacteristics();
            //Task.WaitAll(new Task( async () => await ConnectImpl()));
        }
        private async Task<bool> EnumerateCharacteristics()
        {
            bool b_result = false;
            var attributeInfoDisp = ServiceCollection[ServiceCollection.Count - 1];
            CharacteristicCollection.Clear();
            IReadOnlyList<GattCharacteristic> characteristics = null;
            try
            {
                // Ensure we have access to the device.
                var accessStatus = await attributeInfoDisp.service.RequestAccessAsync();
                if (accessStatus == DeviceAccessStatus.Allowed)
                {
                    // BT_Code: Get all the child characteristics of a service. Use the cache mode to specify uncached characterstics only 
                    // and the new Async functions to get the characteristics of unpaired devices as well. 
                    var result = await attributeInfoDisp.service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
                    if (result.Status == GattCommunicationStatus.Success)
                    {
                        characteristics = result.Characteristics;
                        b_result = true;
                    }
                    else
                    {
                        Console.WriteLine("Error accessing service.");
                        //Debug.Assert(false, "Error accessing service.");

                        // On error, act as if there are no characteristics.
                        characteristics = new List<GattCharacteristic>();
                    }
                }
                else
                {
                    // Not granted access
                    Console.WriteLine("Error accessing service.");
                    //Debug.Assert(false, "Error accessing service.");

                    // On error, act as if there are no characteristics.
                    characteristics = new List<GattCharacteristic>();

                }
            }
            catch (Exception ex)
            {
                //Debug.Assert(false, "Restricted service. Can't read characteristics: " + ex.Message);
                Console.WriteLine("Restricted service. Can't read characteristics: " + ex.Message);
                // On error, act as if there are no characteristics.
                characteristics = new List<GattCharacteristic>();
            }

            foreach (GattCharacteristic c in characteristics)
            {
                CharacteristicCollection.Add(new BluetoothLEAttributeDisplay(c));
            }

            return b_result;
        }

        #endregion

        #region Bytes
        private async Task<bool> WriteBufferToSelectedCharacteristicAsync(IBuffer buffer)
        {
            try
            {
                // BT_Code: Writes the value from the buffer to the characteristic.
                var result = await selectedCharacteristic.WriteValueWithResultAsync(buffer);

                if (result.Status == GattCommunicationStatus.Success)
                {
                    Console.WriteLine("Successfully wrote value to device");

                    if (delayAfterSend != 0)
                    {
                        Console.WriteLine($"Sleeping for {delayAfterSend / 1000} seconds...");
                        Thread.Sleep(delayAfterSend);
                    }

                    return true;
                }
                else
                {
                    Console.WriteLine($"Write failed: {result.Status}");
                    return false;
                }
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_INVALID_PDU)
            {
                Console.WriteLine(ex.Message);
                return false;
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_WRITE_NOT_PERMITTED || ex.HResult == E_ACCESSDENIED)
            {
                // This usually happens when a device reports that it support writing, but it actually doesn't.
                Console.WriteLine(ex.Message);
                return false;
            }
        }

        byte[] StructureToByteArray(object obj)
        {
            int len = Marshal.SizeOf(obj);

            byte[] arr = new byte[len];

            IntPtr ptr = Marshal.AllocHGlobal(len);

            Marshal.StructureToPtr(obj, ptr, true);

            Marshal.Copy(ptr, arr, 0, len);

            Marshal.FreeHGlobal(ptr);

            return arr;
        }


        private async Task<bool> SendByteInChunks(byte[] data)
        {
            Debug.Assert(CharacteristicCollection.Count > WRITE_CHARACTERISTIC_INDEX &&
                         CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].Name == "65523");

            selectedCharacteristic = CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].characteristic;

            uint num_chunks = (uint)Math.Ceiling(((float)data.Length / Constants.OTA_CHUNK_MTU));
            uint offset = 0;

            OTABlob blob = new OTABlob();
            blob.magic = Constants.OTA_BLOB_MAGIC;
            blob.total_size = (ushort)data.Length;
            blob.num_chunks = (byte)num_chunks;
            blob.checksum = 0;
            blob.cur_chunk = 0;

            for (uint chunk = 0; chunk < num_chunks; chunk++)
            {
                uint data_len = Math.Min(Constants.OTA_CHUNK_MTU, (uint)data.Length - offset);
                int struct_size = System.Runtime.InteropServices.Marshal.SizeOf(blob);
                Debug.Assert(struct_size == 12);

                byte[] bytes = new byte[struct_size + data_len];

                //byte[] blob_bytes = StructureToByteArray(blob);
                blob.chunk_len = (ushort)data_len;
                byte[] blob_bytes = blob.ToBytes();

                blob_bytes.CopyTo(bytes, 0);
                //data.CopyTo(bytes, struct_size);

                System.Buffer.BlockCopy(data, (int)offset, bytes, struct_size, (int)data_len);

                var writeBuffer = CryptographicBuffer.CreateFromByteArray(bytes);
                // TODO: add dump buffer to console
                Console.WriteLine($"Chunk {chunk + 1}/{num_chunks} Writing {data_len} bytes to device.");
                var writeSuccessful = await WriteBufferToSelectedCharacteristicAsync(writeBuffer);
                if (!writeSuccessful)
                {
                    return false;
                }

                offset += data_len;
                blob.cur_chunk++;
            }

            return true;
        }

        private async Task SendByteFull(byte[] data)
        {
            Debug.Assert(CharacteristicCollection.Count > WRITE_CHARACTERISTIC_INDEX &&
                         CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].Name == "65523");

            var writeBuffer = CryptographicBuffer.CreateFromByteArray(data);

            selectedCharacteristic = CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].characteristic;

            // TODO: add dump buffer to console
            Console.WriteLine($"Writing data {data[0]} to device.");
            var writeSuccessful = await WriteBufferToSelectedCharacteristicAsync(writeBuffer);
        }


        private async Task<bool> SendByte(byte[] data)
        {
            bool should_use_chunks = true;

            if (should_use_chunks)
            {
                return await SendByteInChunks(data);
            }
            else
            {
                return false;
            }
        }

        private async Task SendByte(string text)
        {
            Debug.Assert(CharacteristicCollection.Count > WRITE_CHARACTERISTIC_INDEX &&
                         CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].Name == "65523");

            var writeBuffer = CryptographicBuffer.ConvertStringToBinary(text,
                BinaryStringEncoding.Utf8);

            selectedCharacteristic = CharacteristicCollection[WRITE_CHARACTERISTIC_INDEX].characteristic;

            Console.WriteLine($"Writing {text} to device.");
            var writeSuccessful = await WriteBufferToSelectedCharacteristicAsync(writeBuffer);
        }

        private async Task<byte[]> ReadByte()
        {
            Debug.Assert(CharacteristicCollection.Count >= READ_CHARACTERISTIC_INDEX &&
                         CharacteristicCollection[READ_CHARACTERISTIC_INDEX].Name == "65524");

            selectedCharacteristic = CharacteristicCollection[READ_CHARACTERISTIC_INDEX].characteristic;
            //Debug.Assert(selectedCharacteristic.PresentationFormats.Count == 1);
            //presentationFormat = selectedCharacteristic.PresentationFormats[0];

            Console.WriteLine($"Reading from device characteristic {READ_CHARACTERISTIC_INDEX}...");
            var readRes = await ReadBufferFromSelectedCharacteristicAsync();
            return readRes;
        }

        private async Task<string> ReadString()
        {
            Debug.Assert(CharacteristicCollection.Count >= 4 && CharacteristicCollection[3].Name == "65524");

            selectedCharacteristic = CharacteristicCollection[3].characteristic;
            //Debug.Assert(selectedCharacteristic.PresentationFormats.Count == 1);
            //presentationFormat = selectedCharacteristic.PresentationFormats[0];

            Console.WriteLine($"Reading from device characteristic 4...");
            var readRes = await ReadStringFromSelectedCharacteristicAsync();
            return readRes;
        }

        private async Task<byte[]> ReadBufferFromSelectedCharacteristicAsync()
        {
            try
            {
                // BT_Code: Writes the value from the buffer to the characteristic.
                GattReadResult result = await selectedCharacteristic.ReadValueAsync(BluetoothCacheMode.Uncached);

                if (result.Status == GattCommunicationStatus.Success)
                {
                    byte[] ret;
                    CryptographicBuffer.CopyToByteArray(result.Value, out ret);
                    Console.WriteLine($"Successfully read {ret[0]} value from device");
                    return ret;
                }
                else
                {
                    Console.WriteLine($"Read failed: {result.Status}");
                    //Debug.Assert(false, $"Read failed: {result.Status}");
                    return null;
                }
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_INVALID_PDU)
            {
                Console.WriteLine(ex.Message);
                //Debug.Assert(false, ex.Message);
                return null;
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_READ_NOT_PERMITTED || ex.HResult == E_ACCESSDENIED)
            {
                // This usually happens when a device reports that it support reading, but it actually doesn't.
                Console.WriteLine(ex.Message);
                //Debug.Assert(false, ex.Message);
                return null;
            }
        }

        private async Task<string> ReadStringFromSelectedCharacteristicAsync()
        {
            try
            {
                // BT_Code: Writes the value from the buffer to the characteristic.
                GattReadResult result = await selectedCharacteristic.ReadValueAsync(BluetoothCacheMode.Uncached);

                if (result.Status == GattCommunicationStatus.Success)
                {
                    string formattedResult = FormatValueByPresentation(result.Value, null);
                    Console.WriteLine($"Successfully read value from device: {formattedResult}");
                    return formattedResult;
                }
                else
                {
                    Console.WriteLine($"Read failed: {result.Status}");
                    //Debug.Assert(false, $"Read failed: {result.Status}");
                    return null;
                }
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_INVALID_PDU)
            {
                Console.WriteLine(ex.Message);
                //Debug.Assert(false, ex.Message);
                return null;
            }
            catch (Exception ex) when (ex.HResult == E_BLUETOOTH_ATT_READ_NOT_PERMITTED || ex.HResult == E_ACCESSDENIED)
            {
                // This usually happens when a device reports that it support reading, but it actually doesn't.
                Console.WriteLine(ex.Message);
                //Debug.Assert(false, ex.Message);
                return null;
            }
        }

        private string FormatValueByPresentation(IBuffer buffer, GattPresentationFormat format)
        {
            // BT_Code: For the purpose of this sample, this function converts only UInt32 and
            // UTF-8 buffers to readable text. It can be extended to support other formats if your app needs them.
            byte[] data;
            CryptographicBuffer.CopyToByteArray(buffer, out data);
            if (false && format.FormatType == GattPresentationFormatTypes.UInt32 && data.Length >= 4)
            {
                return BitConverter.ToInt32(data, 0).ToString();
            }
            //else if (format.FormatType == GattPresentationFormatTypes.Utf8)
            else
            {
                try
                {
                    return Encoding.UTF8.GetString(data);
                }
                catch (ArgumentException)
                {
                    return "(error: Invalid UTF-8 string)";
                }
            }

            return null;
        }

        #endregion

        #region Error Codes
        readonly int E_BLUETOOTH_ATT_READ_NOT_PERMITTED = unchecked((int)0x80650002);
        readonly int E_BLUETOOTH_ATT_WRITE_NOT_PERMITTED = unchecked((int)0x80650003);
        readonly int E_BLUETOOTH_ATT_INVALID_PDU = unchecked((int)0x80650004);
        readonly int E_ACCESSDENIED = unchecked((int)0x80070005);
        readonly int E_DEVICE_NOT_AVAILABLE = unchecked((int)0x800710df); // HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE)
        #endregion
    }
}

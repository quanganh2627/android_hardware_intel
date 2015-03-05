/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: Z:\\android_kernel\\froyo_local\\hardware\\intel\\unipoint_git_cai\\UnipointJavaService\\src\\intel\\aidltest\\aidlsrc\\IUnipointService.aidl
 */
package intel.aidltest.aidlsrc;
public interface IUnipointService extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements intel.aidltest.aidlsrc.IUnipointService
{
private static final java.lang.String DESCRIPTOR = "intel.aidltest.aidlsrc.IUnipointService";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an intel.aidltest.aidlsrc.IUnipointService interface,
 * generating a proxy if needed.
 */
public static intel.aidltest.aidlsrc.IUnipointService asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = (android.os.IInterface)obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof intel.aidltest.aidlsrc.IUnipointService))) {
return ((intel.aidltest.aidlsrc.IUnipointService)iin);
}
return new intel.aidltest.aidlsrc.IUnipointService.Stub.Proxy(obj);
}
public android.os.IBinder asBinder()
{
return this;
}
@Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
{
switch (code)
{
case INTERFACE_TRANSACTION:
{
reply.writeString(DESCRIPTOR);
return true;
}
case TRANSACTION_getCurrentMode:
{
data.enforceInterface(DESCRIPTOR);
int _result = this.getCurrentMode();
reply.writeNoException();
reply.writeInt(_result);
return true;
}
case TRANSACTION_setMode:
{
data.enforceInterface(DESCRIPTOR);
int _arg0;
_arg0 = data.readInt();
this.setMode(_arg0);
reply.writeNoException();
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements intel.aidltest.aidlsrc.IUnipointService
{
private android.os.IBinder mRemote;
Proxy(android.os.IBinder remote)
{
mRemote = remote;
}
public android.os.IBinder asBinder()
{
return mRemote;
}
public java.lang.String getInterfaceDescriptor()
{
return DESCRIPTOR;
}
public int getCurrentMode() throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
int _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
mRemote.transact(Stub.TRANSACTION_getCurrentMode, _data, _reply, 0);
_reply.readException();
_result = _reply.readInt();
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
public void setMode(int mode) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeInt(mode);
mRemote.transact(Stub.TRANSACTION_setMode, _data, _reply, 0);
_reply.readException();
}
finally {
_reply.recycle();
_data.recycle();
}
}
}
static final int TRANSACTION_getCurrentMode = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
static final int TRANSACTION_setMode = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
}
public int getCurrentMode() throws android.os.RemoteException;
public void setMode(int mode) throws android.os.RemoteException;
}

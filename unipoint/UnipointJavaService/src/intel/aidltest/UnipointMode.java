package intel.aidltest;

import android.os.Parcel;
import android.os.Parcelable;

/*
 * This file is not used yet , but will be used if the unipointmode get complex . 
 */
public class UnipointMode implements Parcelable {

	public static int MODE_NORMAL = 1;
	public static int MODE_VOLUME_CONTROL = 2;

	public int mMode;

	public int describeContents() {
		// TODO Auto-generated method stub
		return 0;
	}

	public void writeToParcel(Parcel dest, int flags) {
		dest.writeInt(mMode);

	}

	public static final Parcelable.Creator<UnipointMode> CREATOR = new Parcelable.Creator<UnipointMode>() {

		public UnipointMode createFromParcel(Parcel in) {
			return new UnipointMode(in);
		}

		public UnipointMode[] newArray(int size) {
			return new UnipointMode[size];
		}

	};

	private UnipointMode(Parcel in) {
		mMode = in.readInt();
	}

	public UnipointMode() {

	}

}

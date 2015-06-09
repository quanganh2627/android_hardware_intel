package com.intel.internal.telephony.mmgrtest.adapters;

import com.intel.internal.telephony.mmgrtest.R;
import com.intel.internal.telephony.mmgrtest.fragments.ModemStatusFragment;

import android.content.Context;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;

/**
 * A {@link FragmentPagerAdapter} that returns a fragment corresponding to one
 * of the primary sections of the app.
 */
public class SectionsPagerAdapter extends FragmentPagerAdapter {
    private Context context = null;
    private Fragment[] fragments = new Fragment[] { new ModemStatusFragment() };

    public SectionsPagerAdapter(FragmentManager fm, Context context) {
        super(fm);
        this.context = context;
    }

    @Override
    public Fragment getItem(int i) {
        return this.fragments[i];
    }

    @Override
    public int getCount() {
        return this.fragments.length;
    }

    @Override
    public CharSequence getPageTitle(int position) {
        switch (position) {
        case 0:
            return this.context.getResources()
                   .getString(R.string.title_section1).toUpperCase();
        case 1:
            return this.context.getResources()
                   .getString(R.string.title_section2).toUpperCase();
        case 2:
            return this.context.getResources()
                   .getString(R.string.title_section3).toUpperCase();
        }
        return null;
    }
}

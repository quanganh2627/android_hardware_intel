package com.intel.internal.telephony.mmgrtest.activities;

import com.intel.internal.telephony.mmgrtest.R;
import com.intel.internal.telephony.mmgrtest.adapters.SectionsPagerAdapter;

import android.app.ActionBar;
import android.app.FragmentTransaction;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.view.ViewPager;
import android.view.Menu;

public class MainActivity extends FragmentActivity implements ActionBar.TabListener {
    private SectionsPagerAdapter sectionsPagerAdapter = null;
    private ViewPager viewPager = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        super.setContentView(R.layout.main);
        // Create the adapter that will return a fragment for each of the three
        // primary sections
        // of the app.
        this.sectionsPagerAdapter = new SectionsPagerAdapter(
            super.getSupportFragmentManager(), this);

        // Set up the action bar.
        final ActionBar actionBar = super.getActionBar();
        actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);

        // Set up the ViewPager with the sections adapter.
        this.viewPager = (ViewPager) super.findViewById(R.id.pager);

        if (this.viewPager == null) {
            return;
        }

        this.viewPager.setAdapter(this.sectionsPagerAdapter);

        // When swiping between different sections, select the corresponding
        // tab.
        // We can also use ActionBar.Tab#select() to do this if we have a
        // reference to the
        // Tab.
        this.viewPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener() {
                                                   @Override
                                                   public void onPageSelected(int position) {
                                                       actionBar.setSelectedNavigationItem(position);
                                                   }
                                               });

        // For each of the sections in the app, add a tab to the action bar.
        for (int i = 0; i < sectionsPagerAdapter.getCount(); i++) {
            // Create a tab with text corresponding to the page title defined by
            // the adapter.
            // Also specify this Activity object, which implements the
            // TabListener interface, as the
            // listener for when this tab is selected.
            actionBar.addTab(actionBar.newTab()
                             .setText(this.sectionsPagerAdapter.getPageTitle(i))
                             .setTabListener(this));
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    @Override
    public void onTabUnselected(ActionBar.Tab tab,
                                FragmentTransaction fragmentTransaction) {
    }

    @Override
    public void onTabSelected(ActionBar.Tab tab,
                              FragmentTransaction fragmentTransaction) {
        // When the given tab is selected, switch to the corresponding page in
        // the ViewPager.
        this.viewPager.setCurrentItem(tab.getPosition());
    }

    @Override
    public void onTabReselected(ActionBar.Tab tab,
                                FragmentTransaction fragmentTransaction) {
    }
}

package witviz;



import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.layout.RowLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Dialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;

public class PreferencesDialog extends Dialog {
	final Shell shell;
	final Composite buttonC;
	Button bShowNonDefault;
	Button bHighlightNonDefault;
    Shell parent;
    
	public PreferencesDialog(Shell p, final SimpleTree parentApp) {
		super(p);
	    shell= new Shell(p, SWT.DIALOG_TRIM | SWT.APPLICATION_MODAL );
	    buttonC = new Composite(shell,SWT.BORDER);
	    
	    parent = getParent();
		shell.setText(getText());
		shell.setSize(300,400);
	
		Composite parent = buttonC;
	    bShowNonDefault = new Button(parent,SWT.CHECK);
	    bShowNonDefault.setText("Show only non-default");
	    bShowNonDefault.setSelection(true);
	    parentApp.showOnlyNonDefault=true;
		bShowNonDefault.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				Button b = (Button)e.widget;
				boolean isSelected = b.getSelection();
				parentApp.showOnlyNonDefault=isSelected;
				if (isSelected)
					bHighlightNonDefault.setEnabled(false);
				else
					bHighlightNonDefault.setEnabled(true);
			}
		    public void widgetDefaultSelected(SelectionEvent e) {
			    int foo=1;
			}
		});
		
	    bHighlightNonDefault = new Button(parent,SWT.CHECK);
	    bHighlightNonDefault.setText("Highlight non-default");
	    bHighlightNonDefault.setSelection(true);
	    parentApp.highlightNonDefault=true;
	    bHighlightNonDefault.setEnabled(false);
		bHighlightNonDefault.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				Button b = (Button)e.widget;
				boolean isSelected = b.getSelection();
				parentApp.highlightNonDefault=isSelected;
			}
		    public void widgetDefaultSelected(SelectionEvent e) {
			    int foo=1;
			}
		});
		
		
		
		
		
		shell.addListener(SWT.Close, new Listener(){
			   public void handleEvent(Event event) {
			       event.doit=false;
			       close();
			       //your code here

			    }
			   });	   
	    GridLayout gridLayout1 = new GridLayout();
	    gridLayout1.numColumns = 1;
	    shell.setLayout(gridLayout1);
		
	    GridLayout gridLayout = new GridLayout();
	    gridLayout.numColumns = 2;
	    buttonC.setLayout(gridLayout);
	    
	    Composite selections = new Composite(shell,0);
	    selections.setLayout(new RowLayout());
	    
	    Composite composite = new Composite(shell,0);
		composite.setLayout (new RowLayout ());

		final Button ok = new Button (composite, SWT.PUSH);
		ok.setText ("Ok");
		Button cancel = new Button (composite, SWT.PUSH);
		cancel.setText ("Cancel");
		Listener listener =new Listener () {
			public void handleEvent (Event event) {
				//result [0] = event.widget == ok;
				shell.setVisible (false);
				
			}
		};
		ok.addListener (SWT.Selection, listener);
		cancel.addListener (SWT.Selection, listener);
		shell.pack ();
		shell.setVisible(false);

	}
	public void open () {
		shell.setVisible(true);

		Display display = parent.getDisplay();
		while (!shell.isDisposed()) {
			if (!display.readAndDispatch()) display.sleep();
		}

	}
	public void close() {
		//just make invisible
		shell.setVisible(false);
	}
}
